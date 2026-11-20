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

#include "XdndSource.h"

#include <ui/ClipboardMime.h>

#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <poll.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace coral::ui::detail {

namespace {

constexpr int kXdndVersion = 5;
constexpr auto kPollInterval = std::chrono::milliseconds(30);
constexpr auto kDragTimeout = std::chrono::seconds(30);
constexpr auto kFinishedTimeout = std::chrono::seconds(2);

bool windowHasProperty(Display* display, const ::Window window, const Atom property) {
  Atom type = None;
  int format = 0;
  unsigned long count = 0;
  unsigned long remaining = 0;
  unsigned char* data = nullptr;
  const int status = XGetWindowProperty(display, window, property, 0, 1, False, AnyPropertyType,
    &type, &format, &count, &remaining, &data);
  if (data != nullptr) {
    XFree(data);
  }
  return status == Success && type != None;
}

} // namespace

struct XdndSource::Impl {
  Display* display = nullptr;
  ::Window window = 0;
  ::Window root = 0;
  Atom xdndAware = None;
  Atom xdndEnter = None;
  Atom xdndPosition = None;
  Atom xdndStatus = None;
  Atom xdndLeave = None;
  Atom xdndDrop = None;
  Atom xdndFinished = None;
  Atom xdndSelection = None;
  Atom xdndActionCopy = None;
  Atom xdndTypeList = None;
  Atom targets = None;
  std::vector<Atom> types;
  const DragPayload* payload = nullptr;
  /** The last XdndStatus received, so `run` can tell whether the current target accepts the drop. */
  ::Window statusTarget = 0;
  bool statusAccepted = false;

  void internAtoms() {
    xdndAware = XInternAtom(display, "XdndAware", False);
    xdndEnter = XInternAtom(display, "XdndEnter", False);
    xdndPosition = XInternAtom(display, "XdndPosition", False);
    xdndStatus = XInternAtom(display, "XdndStatus", False);
    xdndLeave = XInternAtom(display, "XdndLeave", False);
    xdndDrop = XInternAtom(display, "XdndDrop", False);
    xdndFinished = XInternAtom(display, "XdndFinished", False);
    xdndSelection = XInternAtom(display, "XdndSelection", False);
    xdndActionCopy = XInternAtom(display, "XdndActionCopy", False);
    xdndTypeList = XInternAtom(display, "XdndTypeList", False);
    targets = XInternAtom(display, "TARGETS", False);
  }

  [[nodiscard]] std::string atomName(const Atom atom) const {
    if (atom == None) return {};
    char* name = XGetAtomName(display, atom);
    if (name == nullptr) return {};
    std::string result(name);
    XFree(name);
    return result;
  }

  /** The deepest window under the pointer that advertises XDND support (root counts as well). */
  [[nodiscard]] ::Window targetAt(const int rootX, const int rootY) const {
    ::Window current = root;
    ::Window aware = windowHasProperty(display, current, xdndAware) ? current : 0;
    for (int guard = 0; guard < 32; ++guard) {
      ::Window rootReturn = 0;
      ::Window child = 0;
      int rootXReturn = 0;
      int rootYReturn = 0;
      int windowX = 0;
      int windowY = 0;
      unsigned int mask = 0;
      if (XQueryPointer(display, current, &rootReturn, &child, &rootXReturn, &rootYReturn, &windowX,
            &windowY, &mask) == False) {
        break;
      }
      if (child == 0) break;
      current = child;
      if (windowHasProperty(display, current, xdndAware)) aware = current;
    }
    (void)rootX;
    (void)rootY;
    return aware;
  }

  void sendEnter(const ::Window target) {
    // Version 5 in the high byte; bit 0 says a longer type list follows in XdndTypeList.
    const long flags = static_cast<long>(kXdndVersion) << 24 | (types.size() > 3 ? 1L : 0L);
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = target;
    event.xclient.message_type = xdndEnter;
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(window);
    event.xclient.data.l[1] = flags;
    for (std::size_t index = 0; index < 3; ++index) {
      event.xclient.data.l[2 + index] =
        index < types.size() ? static_cast<long>(types[index]) : 0;
    }
    XSendEvent(display, target, False, NoEventMask, &event);
    XFlush(display);
  }

  void sendPosition(const ::Window target, const int rootX, const int rootY) {
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = target;
    event.xclient.message_type = xdndPosition;
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(window);
    event.xclient.data.l[1] = 0;
    event.xclient.data.l[2] = static_cast<long>((rootX << 16) | (rootY & 0xFFFF));
    event.xclient.data.l[3] = static_cast<long>(CurrentTime);
    event.xclient.data.l[4] = static_cast<long>(xdndActionCopy);
    XSendEvent(display, target, False, NoEventMask, &event);
    XFlush(display);
  }

  void sendLeave(const ::Window target) {
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = target;
    event.xclient.message_type = xdndLeave;
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(window);
    XSendEvent(display, target, False, NoEventMask, &event);
    XFlush(display);
  }

  void sendDrop(const ::Window target) {
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = target;
    event.xclient.message_type = xdndDrop;
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(window);
    event.xclient.data.l[2] = static_cast<long>(CurrentTime);
    XSendEvent(display, target, False, NoEventMask, &event);
    XFlush(display);
  }

  /** Answers a target's request for the dragged data while the drag runs. */
  void serveSelection(const XSelectionRequestEvent& request) {
    Atom property = request.property != None ? request.property : request.target;
    bool answered = false;
    if (request.selection == xdndSelection && property != None) {
      if (request.target == targets) {
        if (!types.empty()) {
          XChangeProperty(display, request.requestor, property, XA_ATOM, 32, PropModeReplace,
            reinterpret_cast<const unsigned char*>(types.data()), static_cast<int>(types.size()));
          answered = true;
        }
      } else {
        const std::string mime = mimeForSelectionTarget(atomName(request.target));
        const std::string* data = nullptr;
        if (payload != nullptr) {
          if (!payload->type().empty() && mime == payload->type()) {
            data = &payload->data();
          } else if (payload->hasSecondary() && mime == payload->secondaryType()) {
            data = &payload->secondaryData();
          }
        }
        if (data != nullptr) {
          XChangeProperty(display, request.requestor, property, request.target, 8,
            PropModeReplace, reinterpret_cast<const unsigned char*>(data->data()),
            static_cast<int>(data->size()));
          answered = true;
        }
      }
    }
    XEvent notify{};
    notify.xselection.type = SelectionNotify;
    notify.xselection.display = display;
    notify.xselection.requestor = request.requestor;
    notify.xselection.selection = request.selection;
    notify.xselection.target = request.target;
    notify.xselection.property = answered ? property : None;
    notify.xselection.time = request.time;
    XSendEvent(display, request.requestor, False, NoEventMask, &notify);
    XFlush(display);
  }

  /** Processes already-queued events; @return true when the drag reached a terminal state. */
  bool drainEvents(bool* success, bool* escape, bool* release) {
    bool finished = false;
    while (XPending(display) > 0) {
      XEvent event{};
      XNextEvent(display, &event);
      switch (event.type) {
        case SelectionRequest:
          serveSelection(event.xselectionrequest);
          break;
        case ClientMessage:
          if (event.xclient.message_type == xdndFinished) {
            *success = (event.xclient.data.l[1] & 1) != 0;
            finished = true;
          } else if (event.xclient.message_type == xdndStatus) {
            statusTarget = static_cast<::Window>(event.xclient.data.l[0]);
            // Bit 0 of the second word is "the target accepts the drop".
            statusAccepted = (event.xclient.data.l[1] & 1) != 0;
          }
          break;
        case ButtonRelease:
          // The release is the normal end of a drag; the pointer mask below is only a fallback for
          // a release that was never delivered.
          *release = true;
          break;
        case KeyPress:
          if (XLookupKeysym(&event.xkey, 0) == XK_Escape) *escape = true;
          break;
        default:
          break;
      }
    }
    return finished;
  }

  void waitForEvents() const {
    pollfd descriptor{ ConnectionNumber(display), POLLIN, 0 };
    (void)poll(&descriptor, 1, static_cast<int>(kPollInterval.count()));
  }

  [[nodiscard]] DragEffect run(const DragPayload& dragPayload, const DragEffect allowed) {
    if (allowed != DragEffect::Copy) return kNoDragEffect;
    payload = &dragPayload;
    std::vector<std::string> mimes;
    if (!dragPayload.type().empty()) mimes.push_back(dragPayload.type());
    if (dragPayload.hasSecondary()) mimes.push_back(dragPayload.secondaryType());
    for (const std::string& mime: mimes) {
      const std::string_view target = selectionTargetForMime(mime);
      if (target.empty()) continue;
      const Atom atom = XInternAtom(display, std::string(target).c_str(), False);
      if (atom != None && std::find(types.begin(), types.end(), atom) == types.end()) {
        types.push_back(atom);
      }
    }
    if (types.empty()) return kNoDragEffect;

    XSetSelectionOwner(display, xdndSelection, window, CurrentTime);
    if (XGetSelectionOwner(display, xdndSelection) != window) return kNoDragEffect;
    if (types.size() > 3) {
      XChangeProperty(display, window, xdndTypeList, XA_ATOM, 32, PropModeReplace,
        reinterpret_cast<const unsigned char*>(types.data()), static_cast<int>(types.size()));
    }
    const int pointerResult = XGrabPointer(display, window, False,
      PointerMotionMask | ButtonReleaseMask | ButtonPressMask, GrabModeAsync, GrabModeAsync, None,
      None, CurrentTime);
    (void)pointerResult;
    (void)XGrabKeyboard(display, window, False, GrabModeAsync, GrabModeAsync, CurrentTime);
    XFlush(display);

    ::Window currentTarget = 0;
    bool accepted = false;
    bool success = false;
    bool escape = false;
    DragEffect effect = kNoDragEffect;
    const auto deadline = std::chrono::steady_clock::now() + kDragTimeout;
    bool finished = false;
    bool releaseRequested = false;
    bool sawButtonDown = false;

    /** Drops on the current target (or cancels) and waits for its XdndFinished. */
    const auto finishDrag = [&]() {
      if (currentTarget != 0 && accepted) {
        sendDrop(currentTarget);
        const auto finishDeadline = std::chrono::steady_clock::now() + kFinishedTimeout;
        while (!finished && std::chrono::steady_clock::now() < finishDeadline) {
          bool finishSuccess = false;
          bool finishEscape = false;
          bool finishRelease = false;
          if (drainEvents(&finishSuccess, &finishEscape, &finishRelease)) {
            success = finishSuccess;
            finished = true;
            break;
          }
          waitForEvents();
        }
        effect = finished && success ? DragEffect::Copy : kNoDragEffect;
      } else {
        if (currentTarget != 0) sendLeave(currentTarget);
        effect = kNoDragEffect;
      }
      finished = true;
    };

    while (!finished && std::chrono::steady_clock::now() < deadline) {
      bool eventSuccess = false;
      bool eventEscape = false;
      bool eventRelease = false;
      const bool sawFinished = drainEvents(&eventSuccess, &eventEscape, &eventRelease);
      if (eventRelease) releaseRequested = true;
      if (eventEscape) escape = true;
      if (sawFinished) {
        // Only a target that answers XdndDrop sends this, so it is terminal.
        success = eventSuccess;
        finished = true;
        effect = success ? DragEffect::Copy : kNoDragEffect;
        break;
      }
      if (escape) {
        if (currentTarget != 0) sendLeave(currentTarget);
        finished = true;
        break;
      }
      ::Window rootReturn = 0;
      ::Window childReturn = 0;
      int rootX = 0;
      int rootY = 0;
      int windowX = 0;
      int windowY = 0;
      unsigned int mask = 0;
      if (XQueryPointer(display, root, &rootReturn, &childReturn, &rootX, &rootY, &windowX, &windowY,
            &mask) == False) {
        break;
      }
      const ::Window target = targetAt(rootX, rootY);
      if (target != currentTarget) {
        if (currentTarget != 0) sendLeave(currentTarget);
        currentTarget = target;
        accepted = false;
        statusTarget = 0;
        statusAccepted = false;
        if (currentTarget != 0) sendEnter(currentTarget);
      }
      if (currentTarget != 0) {
        // The target answers with XdndStatus; ask again on every move so a target that changed its
        // mind (a modifier, a different area) can accept or refuse.
        sendPosition(currentTarget, rootX, rootY);
        if (statusTarget == currentTarget) accepted = statusAccepted;
      }
      if ((mask & Button1Mask) != 0) {
        sawButtonDown = true;
      } else if (sawButtonDown) {
        // The release event was missed; the pointer state is the fallback.
        releaseRequested = true;
      }
      if (releaseRequested) {
        finishDrag();
        break;
      }
      waitForEvents();
    }
    if (!finished && currentTarget != 0) sendLeave(currentTarget);
    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);
    XDeleteProperty(display, window, xdndTypeList);
    XFlush(display);
    payload = nullptr;
    return effect;
  }
};

XdndSource::XdndSource(Display* display, const ::Window window)
    : mDisplay(display), mWindow(window), mImpl(std::make_unique<Impl>()) {
  mImpl->display = display;
  mImpl->window = window;
  mImpl->root = DefaultRootWindow(display);
  mImpl->internAtoms();
}

XdndSource::~XdndSource() = default;

DragEffect XdndSource::run(const DragPayload& payload, const DragEffect allowed) {
  if (mDisplay == nullptr || mWindow == 0) return kNoDragEffect;
  return mImpl->run(payload, allowed);
}

} // namespace coral::ui::detail
