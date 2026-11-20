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

// X11 clipboard smoke test: multiple MIME representations over a real selection.
//
// `test_ui` runs headless on CI, so the parts that need a display and a live selection owner cannot
// live there. This executable exercises two independent X11 clipboard connections (one owner, one
// requester) against a real X server and returns 77 when no display is available, so CTest reports a
// skip instead of a pass. CI runs it under `xvfb-run`.

#include "platform/linux/ClipboardLinuxBackends.h"

#include <ui/Clipboard.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void check(const bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) {
    ++failures;
  }
}

bool contains(const std::vector<std::string>& values, const std::string& value) {
  for (const auto& entry: values) {
    if (entry == value) {
      return true;
    }
  }
  return false;
}

/** Polls a condition on the calling thread; the backends answer on their own worker threads. */
bool waitFor(const std::function<bool()>& condition,
    const std::chrono::milliseconds limit = std::chrono::seconds(3)) {
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline) {
    if (condition()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return condition();
}

} // namespace

int main() {
  using namespace coral::ui;

  if (std::getenv("DISPLAY") == nullptr) {
    std::fprintf(stderr, "SKIP: no X11 display in this session\n");
    return 77;
  }
  auto owner = detail::createX11Clipboard();
  auto requester = detail::createX11Clipboard();
  if (owner == nullptr || requester == nullptr) {
    std::fprintf(stderr, "SKIP: the X11 clipboard backend could not open a display\n");
    return 77;
  }

  // PNG is a byte stream: the embedded NUL proves the protocol path moves bytes, not a C string.
  const std::string png{ '\x89', 'P', 'N', 'G', '\r', '\n', '\x1a', '\n', '\0', '\x01', '\x02' };
  const std::string html = "<b>coral</b>";
  const std::string uris = "file:///tmp/a.mp4\r\nfile:///tmp/b.mp4";

  check(owner->set("text/plain", "hello"), "the owner publishes text/plain");
  check(owner->set("image/png", png), "the owner publishes image/png");
  check(owner->set("text/html", html), "the owner publishes text/html");
  check(owner->set("text/uri-list", uris), "the owner publishes text/uri-list");
  check(owner->get("image/png") == png, "the owner reads its own payload without a conversion");

  const std::vector<std::string> formats = requester->formats();
  check(contains(formats, "text/plain"), "the requester sees text/plain in TARGETS");
  check(contains(formats, "image/png"), "the requester sees image/png in TARGETS");
  check(contains(formats, "text/html"), "the requester sees text/html in TARGETS");
  check(contains(formats, "text/uri-list"), "the requester sees text/uri-list in TARGETS");

  check(requester->get("image/png") == png, "the requester receives image/png with its NUL byte");
  check(requester->get("text/html") == html, "the requester receives text/html");
  check(requester->get("text/plain") == "hello", "the requester receives text/plain");
  const std::vector<std::string> files = requester->getFiles();
  check(files.size() == 2 && files[0] == "file:///tmp/a.mp4" && files[1] == "file:///tmp/b.mp4",
    "the requester receives text/uri-list as paths");
  check(requester->get("application/x-coral-missing").empty(),
    "a MIME the owner does not hold reads as empty");

  // Change notification: the owner writing again must reach the requester without a read on its part.
  std::atomic<int> changes{ 0 };
  requester->setChangeHandler([&changes] { changes.fetch_add(1); });
#if defined(CORAL_UI_TEST_XFIXES)
  const uint64_t before = Clipboard::systemChangeCount();
#endif
  check(owner->set("text/plain", "second"), "the owner writes text/plain again");
#if defined(CORAL_UI_TEST_XFIXES)
  check(waitFor([&changes] { return changes.load() > 0; }),
    "the requester is notified of a foreign selection change (XFixes)");
  check(Clipboard::systemChangeCount() > before, "systemChangeCount advances on a foreign change");
#else
  std::printf("SKIP  this build has no XFixes; change notification falls back to owner polling\n");
#endif
  check(requester->get("text/plain") == "second", "the requester reads the new selection");

  // Ownership handover: the previous owner must drop its payload and re-read from the new owner.
  check(requester->set("text/plain", "requester-wins"), "the requester takes the selection");
  check(requester->get("text/plain") == "requester-wins", "the new owner reads its own value");
  // `set` is queued to the backend worker; wait for the server to see the new owner before the owner
  // takes the selection back, otherwise the two XSetSelectionOwner requests race.
  check(waitFor([&owner] { return owner->get("text/plain") == "requester-wins"; }),
    "the selection moved to the requester");
  const int changesBeforeTakeBack = changes.load();
  check(owner->set("text/plain", "third"), "the owner takes the selection back");
  check(waitFor([&changes, changesBeforeTakeBack] { return changes.load() > changesBeforeTakeBack; }),
    "the old owner notices the selection move away");
  check(requester->get("text/plain") == "third",
    "the old owner re-reads from the new owner instead of serving cached bytes");
  const std::vector<std::string> afterHandover = requester->formats();
  check(afterHandover.size() == 1 && contains(afterHandover, "text/plain"),
    "the new owner advertises only what it re-set (the old payload was dropped)");

  // INCR: a payload beyond one X request is transferred in chunks. The threshold is lowered so a
  // modest payload exercises the same protocol path without multi-megabyte test data.
  detail::setX11ClipboardIncrThresholdForTesting(4096);
  std::string blob(256 * 1024, '\0');
  for (size_t i = 0; i < blob.size(); ++i) {
    blob[i] = static_cast<char>((i * 131 + 7) & 0xFF);
  }
  check(owner->set("application/x-coral-blob", blob), "the owner publishes a 256 KiB payload");
  check(contains(requester->formats(), "application/x-coral-blob"),
    "the requester sees the large MIME in TARGETS");
  const std::string received = requester->get("application/x-coral-blob");
  check(received.size() == blob.size() && received == blob,
    "a large payload arrives intact through INCR");
  detail::setX11ClipboardIncrThresholdForTesting(0);

  // PRIMARY: a second selection that follows the mouse. It must work over the same connection and
  // stay independent of the CLIPBOARD payload.
  check(owner->setPrimary("text/plain", "marked text"), "the owner publishes the PRIMARY selection");
  check(owner->getPrimary("text/plain") == "marked text", "the owner reads its own PRIMARY locally");
  check(requester->getPrimary("text/plain") == "marked text",
    "the requester reads PRIMARY through a selection conversion");
  check(contains(requester->primaryFormats(), "text/plain"), "the requester lists PRIMARY formats");
  check(requester->getText() == "third", "publishing PRIMARY leaves CLIPBOARD untouched");
  check(requester->setPrimary("text/plain", "requester primary"),
    "the requester takes the PRIMARY selection");
  check(requester->getText() == "third", "a PRIMARY write leaves the CLIPBOARD owner alone");
  check(owner->getPrimary("text/plain") == "requester primary",
    "the old PRIMARY owner re-reads from the new one");

  // No owner at all: empty and fast, not a one second conversion timeout.
  owner.reset();
  requester.reset();
  auto reader = detail::createX11Clipboard();
  if (reader == nullptr) {
    std::fprintf(stderr, "FAIL: a fresh X11 clipboard could not be created\n");
    return 1;
  }
  const auto started = std::chrono::steady_clock::now();
  const bool emptyFormats = reader->formats().empty();
  const bool emptyText = reader->get("text/plain").empty();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - started);
  check(emptyFormats && emptyText, "an unowned selection reads as empty");
  check(reader->getPrimary("text/plain").empty(), "an unowned PRIMARY selection reads as empty");
  check(elapsed.count() < 900, "an unowned selection does not wait for the conversion timeout");

  std::printf("%s (%d failure(s))\n", failures == 0 ? "ALL PASS" : "FAILURES", failures);
  return failures == 0 ? 0 : 1;
}
