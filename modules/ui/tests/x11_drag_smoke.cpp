/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// X11 XDND drag-out smoke test.
//
// The drag source blocks and runs its own event loop, so it needs a second client to talk to. This
// executable opens two X11 connections: `XdndSource` drives one, and a small fake target on the other
// answers XdndEnter/Position, accepts the drop, requests the selection and reports the bytes it got.
// It returns 77 without a display so CTest reports a skip; CI runs it under `xvfb-run`.

#include "platform/linux/XdndSource.h"

#include <ui/DragDrop.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <poll.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace {

struct TargetState {
  Display* display = nullptr;
  ::Window window = 0;
  bool accept = true;
  Atom xdndEnter = None;
  Atom xdndPosition = None;
  Atom xdndDrop = None;
  Atom xdndLeave = None;
  Atom xdndStatus = None;
  Atom xdndFinished = None;
  Atom xdndSelection = None;
  Atom xdndActionCopy = None;
  Atom property = None;
  Atom utf8 = None;
  ::Window source = 0;
  std::mutex mutex;
  std::string received;
  bool finished = false;
  bool success = false;
  /** Whether the synthetic release for this drag was already sent. */
  bool releaseSent = false;
};

void sendStatus(TargetState& target, const bool accept) {
  XEvent event{};
  event.xclient.type = ClientMessage;
  event.xclient.display = target.display;
  event.xclient.window = target.source;
  event.xclient.message_type = target.xdndStatus;
  event.xclient.format = 32;
  event.xclient.data.l[0] = static_cast<long>(target.window);
  event.xclient.data.l[1] = accept ? 1 : 0;
  event.xclient.data.l[4] = static_cast<long>(target.xdndActionCopy);
  XSendEvent(target.display, target.source, False, NoEventMask, &event);
  XFlush(target.display);
}

void sendFinished(TargetState& target, const bool success) {
  XEvent event{};
  event.xclient.type = ClientMessage;
  event.xclient.display = target.display;
  event.xclient.window = target.source;
  event.xclient.message_type = target.xdndFinished;
  event.xclient.format = 32;
  event.xclient.data.l[0] = static_cast<long>(target.window);
  event.xclient.data.l[1] = success ? 1 : 0;
  event.xclient.data.l[4] = static_cast<long>(target.xdndActionCopy);
  XSendEvent(target.display, target.source, False, NoEventMask, &event);
  XFlush(target.display);
}

/** Runs the fake target until @p stop is set; mirrors what a real target does with a text drop. */
void targetLoop(TargetState& target, std::atomic<bool>& stop) {
  while (!stop.load()) {
    while (XPending(target.display) > 0) {
      XEvent event{};
      XNextEvent(target.display, &event);
      if (event.type == ClientMessage) {
        const XClientMessageEvent& message = event.xclient;
        if (message.message_type == target.xdndEnter) {
          target.source = static_cast<::Window>(message.data.l[0]);
        } else if (message.message_type == target.xdndPosition) {
          target.source = static_cast<::Window>(message.data.l[0]);
          sendStatus(target, target.accept);
          // The source ends the drag on the button release. This test has no XTest headers, so the
          // fake target synthesises the release once it has accepted the position; the protocol path
          // afterwards (drop, selection request, data, finished) is the real one.
          if (target.accept && !target.releaseSent) {
            target.releaseSent = true;
            XEvent release{};
            release.xbutton.type = ButtonRelease;
            release.xbutton.display = target.display;
            release.xbutton.window = target.source;
            release.xbutton.root = DefaultRootWindow(target.display);
            release.xbutton.button = Button1;
            release.xbutton.state = Button1Mask;
            XSendEvent(target.display, target.source, False, NoEventMask, &release);
            XFlush(target.display);
          }
        } else if (message.message_type == target.xdndDrop) {
          target.source = static_cast<::Window>(message.data.l[0]);
          XConvertSelection(target.display, target.xdndSelection, target.utf8, target.property,
            target.window, static_cast<Time>(message.data.l[2]));
          XFlush(target.display);
        }
        continue;
      }
      if (event.type != SelectionNotify) continue;
      std::string text;
      if (event.xselection.property != None) {
        Atom type = None;
        int format = 0;
        unsigned long count = 0;
        unsigned long remaining = 0;
        unsigned char* data = nullptr;
        if (XGetWindowProperty(target.display, target.window, event.xselection.property, 0,
              1u << 20, False, AnyPropertyType, &type, &format, &count, &remaining, &data) ==
              Success &&
            data != nullptr) {
          text.assign(reinterpret_cast<const char*>(data), count);
          XFree(data);
        }
        XDeleteProperty(target.display, target.window, event.xselection.property);
      }
      sendFinished(target, target.accept && !text.empty());
      const std::lock_guard<std::mutex> lock(target.mutex);
      target.received = std::move(text);
      target.finished = true;
    }
    pollfd descriptor{ ConnectionNumber(target.display), POLLIN, 0 };
    (void)poll(&descriptor, 1, 10);
  }
}

/** Waits for the target thread to record the drop; false on timeout. */
bool waitForFinished(TargetState& target) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    {
      const std::lock_guard<std::mutex> lock(target.mutex);
      if (target.finished) return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

int failures = 0;

void check(const bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

} // namespace

int main() {
  Display* sourceDisplay = XOpenDisplay(nullptr);
  Display* targetDisplay = XOpenDisplay(nullptr);
  if (sourceDisplay == nullptr || targetDisplay == nullptr) {
    std::fprintf(stderr, "SKIP: no X11 display in this session\n");
    return 77;
  }
  const ::Window sourceRoot = DefaultRootWindow(sourceDisplay);
  const ::Window targetRoot = DefaultRootWindow(targetDisplay);
  const ::Window sourceWindow =
    XCreateSimpleWindow(sourceDisplay, sourceRoot, 300, 300, 8, 8, 0, 0, 0);
  const ::Window targetWindow =
    XCreateSimpleWindow(targetDisplay, targetRoot, 0, 0, 200, 200, 0, 0, 0);
  XMapWindow(sourceDisplay, sourceWindow);

  TargetState target;
  target.display = targetDisplay;
  target.window = targetWindow;
  target.xdndEnter = XInternAtom(targetDisplay, "XdndEnter", False);
  target.xdndPosition = XInternAtom(targetDisplay, "XdndPosition", False);
  target.xdndDrop = XInternAtom(targetDisplay, "XdndDrop", False);
  target.xdndLeave = XInternAtom(targetDisplay, "XdndLeave", False);
  target.xdndStatus = XInternAtom(targetDisplay, "XdndStatus", False);
  target.xdndFinished = XInternAtom(targetDisplay, "XdndFinished", False);
  target.xdndSelection = XInternAtom(targetDisplay, "XdndSelection", False);
  target.xdndActionCopy = XInternAtom(targetDisplay, "XdndActionCopy", False);
  target.property = XInternAtom(targetDisplay, "CORAL_XDND_TEST", False);
  target.utf8 = XInternAtom(targetDisplay, "UTF8_STRING", False);

  // Declare the fake window as an XDND target (version 5).
  const Atom xdndAware = XInternAtom(targetDisplay, "XdndAware", False);
  const long version = 5;
  XChangeProperty(targetDisplay, targetWindow, xdndAware, XA_ATOM, 32, PropModeReplace,
    reinterpret_cast<const unsigned char*>(&version), 1);
  XMapWindow(targetDisplay, targetWindow);
  XSync(targetDisplay, False);
  XSync(sourceDisplay, False);

  std::atomic<bool> stop{ false };
  std::thread targetThread([&] { targetLoop(target, stop); });

  // The pointer must be over the target for the source to find it; both connections see one server.
  XWarpPointer(sourceDisplay, None, sourceRoot, 0, 0, 0, 0, 100, 100);
  XSync(sourceDisplay, False);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  {
    const auto payload = coral::ui::DragPayload::text("coral xdnd payload");
    coral::ui::detail::XdndSource source(sourceDisplay, sourceWindow);
    const coral::ui::DragEffect effect = source.run(payload, coral::ui::DragEffect::Copy);
    check(effect == coral::ui::DragEffect::Copy, "an accepted drop reports the copy effect");
    check(waitForFinished(target), "the target received the selection notification");
    std::string received;
    {
      const std::lock_guard<std::mutex> lock(target.mutex);
      received = target.received;
    }
    check(received == "coral xdnd payload", "the target received the dragged text");
  }

  // A target that rejects the position must never receive the drop.
  target.accept = false;
  {
    const std::lock_guard<std::mutex> lock(target.mutex);
    target.finished = false;
    target.success = false;
    target.received.clear();
    target.releaseSent = false;
  }
  {
    const auto payload = coral::ui::DragPayload::text("rejected payload");
    coral::ui::detail::XdndSource source(sourceDisplay, sourceWindow);
    const coral::ui::DragEffect effect = source.run(payload, coral::ui::DragEffect::Copy);
    check(effect == coral::ui::kNoDragEffect, "a rejected drag reports no effect");
    check(!waitForFinished(target), "a rejected drag sends no drop to the target");
  }

  stop.store(true);
  targetThread.join();
  XDestroyWindow(sourceDisplay, sourceWindow);
  XDestroyWindow(targetDisplay, targetWindow);
  XCloseDisplay(sourceDisplay);
  XCloseDisplay(targetDisplay);

  std::printf("%s (%d failure(s))\n", failures == 0 ? "ALL PASS" : "FAILURES", failures);
  return failures == 0 ? 0 : 1;
}
