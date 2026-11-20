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

#include "ClipboardLinuxBackends.h"

#include <ui/Clipboard.h>
#include <ui/ClipboardMime.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#if defined(CORAL_UI_HAS_XFIXES)
#include <X11/extensions/Xfixes.h>
#endif

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace coral::ui::detail {

namespace {

constexpr int kPollTimeoutMs = 100;
/** A conversion either answers immediately or is a hung owner; a chunked transfer finishes in ms. */
constexpr auto kReadTimeout = std::chrono::seconds(3);
/** Chunk size for an incremental (INCR) transfer; small enough for every server's max request size. */
constexpr size_t kIncrChunkBytes = 64 * 1024;
/** Drops an INCR transfer whose requestor stopped deleting the property instead of leaking it. */
constexpr auto kIncrTransferTimeout = std::chrono::seconds(60);

/**
 * Test-only override of the owner's INCR threshold; 0 means "use the server's single-request limit".
 *
 * The smoke test sets a low value so a modest payload exercises the incremental path deterministically
 * instead of needing one larger than the server's (possibly 16 MiB) request limit.
 */
std::atomic<size_t> gIncrThresholdOverride{ 0 };

/** One completed selection conversion; a TARGETS answer fills `atoms` instead of `bytes`. */
struct Conversion {
  std::string bytes;
  std::vector<unsigned long> atoms;
  Atom type = None;
  int format = 0;
  bool ok = false;
};

/** An owner-side INCR transfer waiting for the requestor to delete the property chunk by chunk. */
struct OutgoingTransfer {
  Window requestor = 0;
  Atom property = None;
  Atom type = None;
  std::string data;
  size_t offset = 0;
  /** All data chunks were written; the next delete asks for the zero-length terminator. */
  bool dataComplete = false;
  std::chrono::steady_clock::time_point deadline;
};

/**
 * Reads a whole X11 property, following the offset until the server reports no remainder.
 *
 * The request length is in 32-bit units, so an 8-bit payload moves in 1 MiB slices. Large selections
 * are transferred with the ICCCM INCR protocol before this is reached (see the class comment).
 */
bool readProperty(Display* display, const Window window, const Atom property, Conversion& out) {
  out.bytes.clear();
  out.atoms.clear();
  long offset = 0;
  for (int guard = 0; guard < 1024; ++guard) {
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(display, window, property, offset, 1 << 18, False,
      AnyPropertyType, &type, &format, &count, &remaining, &data);
    if (status != Success) {
      return false;
    }
    out.type = type;
    out.format = format;
    if (data != nullptr) {
      if (format == 8) {
        out.bytes.append(reinterpret_cast<const char*>(data), count);
      } else if (format == 32) {
        const auto* words = reinterpret_cast<const unsigned long*>(data);
        out.atoms.insert(out.atoms.end(), words, words + count);
      }
      XFree(data);
    }
    if (remaining == 0) {
      out.ok = true;
      return true;
    }
    offset += static_cast<long>(out.bytes.size() / 4 + out.atoms.size());
  }
  return false;
}

/**
 * An X11 CLIPBOARD owner with several MIME representations.
 *
 * X11 selection ownership is a live capability: the owner must answer SelectionRequest events for as
 * long as it owns the selection, so the connection and event loop live on a small worker thread. The
 * public Clipboard methods stay synchronous and communicate with that thread through a pipe.
 *
 * Formats are MIME types. On the wire they become selection targets: `text/plain` is advertised as
 * `UTF8_STRING` (plus legacy `TEXT`/`STRING`), and every other MIME is advertised under its own name
 * (`text/html`, `text/uri-list`, `image/png`, `application/x-...`); `mimeForSelectionTarget` /
 * `selectionTargetForMime` own that translation so the owner and the requester cannot drift apart.
 *
 * Change notification: with XFixes the owner subscribes to selection-owner changes on the CLIPBOARD
 * atom, which reports writes from any process (the X11 twin of Win32's WM_CLIPBOARDUPDATE). Without
 * XFixes the event loop polls the owner and reports a change when a different window took the
 * selection; a second copy by the same owner is invisible in that fallback.
 *
 * PRIMARY: the same owner also serves the PRIMARY selection ("select to copy"), with its own payload
 * and target list, so middle-click paste in another application gets what this one marked. It
 * deliberately has no change notification, matching the platform convention that the primary
 * selection follows the mouse silently.
 *
 * Large payloads: a representation that does not fit into one XChangeProperty request is transferred
 * with the ICCCM INCR protocol in both directions (the owner writes the property again each time the
 * requestor deletes it; the requester accumulates PropertyNotify chunks until the zero-length one).
 * Without it a multi-megabyte PNG or HTML clip failed outright, because the property write exceeded
 * the server's maximum request size.
 */
class X11Clipboard final : public Clipboard {
 public:
  X11Clipboard() {
    XInitThreads();
    mDisplay = XOpenDisplay(nullptr);
    if (mDisplay == nullptr) {
      return;
    }
    const int screen = DefaultScreen(mDisplay);
    mRoot = RootWindow(mDisplay, screen);
    mWindow = XCreateSimpleWindow(mDisplay, mRoot, 0, 0, 1, 1, 0, 0, 0);
    if (mWindow == 0) {
      XCloseDisplay(mDisplay);
      mDisplay = nullptr;
      return;
    }
    XSelectInput(mDisplay, mWindow, PropertyChangeMask);
    mClipboard = XInternAtom(mDisplay, "CLIPBOARD", False);
    mPrimary = XInternAtom(mDisplay, "PRIMARY", False);
    mTargets = XInternAtom(mDisplay, "TARGETS", False);
    mProperty = XInternAtom(mDisplay, "CORAL_CLIPBOARD", False);
    mIncr = XInternAtom(mDisplay, "INCR", False);
    mReadSelection = mClipboard;
    // A property write has to stay below the server's request limit; INCR starts just above it. The
    // slack covers the request header, the property name and padding. A server that reports an
    // unusably small limit (0 here) makes every non-empty payload incremental, which is always safe.
    const long extended = XExtendedMaxRequestSize(mDisplay);
    const long units = extended > 0 ? extended : XMaxRequestSize(mDisplay);
    const long bytes = units > 0 ? units * 4 : 0;
    mIncrThreshold = bytes > 2048 ? static_cast<size_t>(bytes - 1024) : 0;
#if defined(CORAL_UI_HAS_XFIXES)
    int eventBase = 0;
    int errorBase = 0;
    if (XFixesQueryExtension(mDisplay, &eventBase, &errorBase) != 0) {
      mXfixesEventBase = eventBase;
      XFixesSelectSelectionInput(mDisplay, mRoot, mClipboard,
        XFixesSetSelectionOwnerNotifyMask | XFixesSelectionWindowDestroyNotifyMask |
          XFixesSelectionClientCloseNotifyMask);
    }
#endif
    if (pipe(mPipe) != 0) {
      XDestroyWindow(mDisplay, mWindow);
      XCloseDisplay(mDisplay);
      mDisplay = nullptr;
      return;
    }
    mRunning = true;
    mThread = std::thread([this] { run(); });
  }

  ~X11Clipboard() override {
    if (mDisplay == nullptr) {
      return;
    }
    mRunning = false;
    wake();
    if (mThread.joinable()) {
      mThread.join();
    }
    close(mPipe[0]);
    close(mPipe[1]);
    XDestroyWindow(mDisplay, mWindow);
    XCloseDisplay(mDisplay);
  }

  [[nodiscard]] bool valid() const noexcept {
    return mDisplay != nullptr;
  }

  [[nodiscard]] std::string get(const std::string& mime) const override {
    return readMime(mClipboard, mime);
  }

  bool set(const std::string& mime, std::string data) override {
    return setSelection(mClipboard, mime, std::move(data), /*notify*/ true);
  }

  [[nodiscard]] std::vector<std::string> formats() const override {
    return readFormats(mClipboard);
  }

  [[nodiscard]] std::string getPrimary(const std::string& mime) const override {
    return readMime(mPrimary, mime);
  }

  bool setPrimary(const std::string& mime, std::string data) override {
    // PRIMARY is not the clipboard: no change notification, and the owner only has to answer
    // selection requests for as long as it owns it.
    return setSelection(mPrimary, mime, std::move(data), /*notify*/ false);
  }

  [[nodiscard]] std::vector<std::string> primaryFormats() const override {
    return readFormats(mPrimary);
  }

 private:
  [[nodiscard]] ClipboardPayload& payloadFor(const Atom selection) {
    return selection == mPrimary ? mPrimaryPayload : mPayload;
  }

  [[nodiscard]] const ClipboardPayload& payloadFor(const Atom selection) const {
    return selection == mPrimary ? mPrimaryPayload : mPayload;
  }

  /**
   * Whether this window currently owns @p selection.
   *
   * CLIPBOARD ownership is tracked through XFixes/polling (no round trip per read), while PRIMARY has
   * no change notification and asks the server, so a copy made by another application is never
   * mistaken for our own payload.
   */
  [[nodiscard]] bool ownsSelection(const Atom selection) const {
    {
      const std::lock_guard lock(mMutex);
      if (selection == mClipboard ? mOwned : mPrimaryPendingOwner) {
        return true;
      }
      if (selection == mClipboard) {
        return false;
      }
    }
    return mDisplay != nullptr && XGetSelectionOwner(mDisplay, selection) == mWindow;
  }

  [[nodiscard]] std::string readMime(const Atom selection, const std::string& mime) const {
    if (mDisplay == nullptr || mime.empty()) {
      return {};
    }
    if (ownsSelection(selection)) {
      // Reading our own selection through the server would queue a request for this same thread.
      const std::lock_guard lock(mMutex);
      return payloadFor(selection).get(mime);
    }
    const std::string_view target = selectionTargetForMime(mime);
    if (!target.empty()) {
      Conversion result = request(selection, internTarget(target));
      if (result.ok) {
        return result.bytes;
      }
    }
    if (mime == "text/plain") {
      // Older owners only answer the legacy string targets, not UTF8_STRING.
      static constexpr std::string_view kLegacyTargets[] = { "TEXT", "STRING" };
      for (const std::string_view legacy: kLegacyTargets) {
        Conversion result = request(selection, internTarget(legacy));
        if (result.ok) {
          return result.bytes;
        }
      }
    }
    return {};
  }

  bool setSelection(const Atom selection, const std::string& mime, std::string data,
      const bool notify) {
    if (mDisplay == nullptr || mime.empty() || selectionTargetForMime(mime).empty()) {
      return false;
    }
    {
      const std::lock_guard lock(mMutex);
      payloadFor(selection).set(mime, std::move(data));
      if (selection == mClipboard) {
        mOwned = true;
        mPendingOwner = true;
      } else {
        mPrimaryPendingOwner = true;
      }
    }
    if (notify) {
      notePlatformClipboardChanged();
      notifyChanged();
    }
    wake();
    return true;
  }

  [[nodiscard]] std::vector<std::string> readFormats(const Atom selection) const {
    if (mDisplay == nullptr) {
      return {};
    }
    if (ownsSelection(selection)) {
      const std::lock_guard lock(mMutex);
      return payloadFor(selection).formats();
    }
    const Conversion targets = request(selection, mTargets);
    if (!targets.ok || targets.format != 32) {
      return {};
    }
    std::vector<std::string> result;
    for (const unsigned long word: targets.atoms) {
      const std::string mime = mimeForSelectionTarget(atomName(static_cast<Atom>(word)));
      if (mime.empty()) {
        continue;
      }
      if (std::find(result.begin(), result.end(), mime) == result.end()) {
        result.push_back(mime);
      }
    }
    return result;
  }

 private:
  void wake() const {
    if (mPipe[1] >= 0) {
      const char byte = 1;
      (void)!write(mPipe[1], &byte, 1);
    }
  }

  /** Interns (and caches) the atom for a selection target name. */
  Atom internTarget(const std::string_view name) const {
    const std::lock_guard lock(mMutex);
    const auto found = mAtoms.find(std::string(name));
    if (found != mAtoms.end()) {
      return found->second;
    }
    const Atom atom = XInternAtom(mDisplay, std::string(name).c_str(), False);
    mAtoms.emplace(std::string(name), atom);
    return atom;
  }

  [[nodiscard]] std::string atomName(const Atom atom) const {
    char* name = XGetAtomName(mDisplay, atom);
    if (name == nullptr) {
      return {};
    }
    std::string result(name);
    XFree(name);
    return result;
  }

  /**
   * Asks the current owner for one target and waits for the SelectionNotify (and, for a large
   * selection, for the whole INCR transfer).
   *
   * Called from the change handler (which runs on the worker thread) this would wait for a conversion
   * that only this thread can answer, so it reports absence instead of stalling the event loop.
   */
  [[nodiscard]] Conversion request(const Atom selection, const Atom target) const {
    if (mDisplay == nullptr || selection == None || target == None) {
      return {};
    }
    if (std::this_thread::get_id() == mThread.get_id()) {
      return {};
    }
    std::unique_lock lock(mMutex);
    mReadResult = {};
    mReadSelection = selection;
    mReadTarget = target;
    mReadDone = false;
    mReadRequest = true;
    // Drop any partial transfer left by a previous timeout so its chunks cannot leak into this read.
    mIncrActive = false;
    mIncrData.clear();
    wake();
    const auto deadline = std::chrono::steady_clock::now() + kReadTimeout;
    if (!mCv.wait_until(lock, deadline, [this] { return mReadDone; })) {
      mIncrActive = false;
      mIncrData.clear();
      return {};
    }
    return std::move(mReadResult);
  }

  void setOwner(const Atom selection) {
    if (mDisplay == nullptr || mWindow == 0) {
      return;
    }
    XSetSelectionOwner(mDisplay, selection, mWindow, CurrentTime);
    XFlush(mDisplay);
    if (selection == mClipboard) {
      const std::lock_guard lock(mMutex);
      mLastOwner = mWindow;
    }
  }

  void beginRead() {
    Atom target = None;
    Atom selection = mClipboard;
    {
      const std::lock_guard lock(mMutex);
      target = mReadTarget;
      selection = mReadSelection;
    }
    if (target == None) {
      return;
    }
    XConvertSelection(mDisplay, selection, target, mProperty, mWindow, CurrentTime);
    XFlush(mDisplay);
  }

  /**
   * Records a selection owner that is not this window; a real change, not our own claim.
   *
   * `timestamp` separates two consecutive copies by the same owner (0 when only the owner is known).
   */
  void applyForeignOwner(const Window owner, const Time timestamp) {
    bool changed = false;
    {
      const std::lock_guard lock(mMutex);
      if (mPendingOwner) {
        // A local `set` has not reached the server yet (the request is only sent when the worker
        // picks it up), so any owner event read now describes the clipboard before that set.
        return;
      }
      if (owner == mWindow) {
        mLastOwner = owner;
        mLastSelectionTime = timestamp;
        return;
      }
      if (owner == mLastOwner && timestamp == mLastSelectionTime) {
        return;
      }
      mLastOwner = owner;
      mLastSelectionTime = timestamp;
      mOwned = false;
      mPayload.clear();
      changed = true;
    }
    if (changed) {
      notePlatformClipboardChanged();
      notifyChanged();
    }
  }

  /**
   * Handles an XFixes owner notification.
   *
   * The server's *current* owner decides, not the event's: an event queued just before our own
   * `set` would otherwise report the previous owner after the payload was already replaced, and
   * clearing it there would leave us owning an empty clipboard.
   */
  void handleOwnerNotification(const Time timestamp) {
    if (mDisplay == nullptr) {
      return;
    }
    const Window current = XGetSelectionOwner(mDisplay, mClipboard);
    if (current == mWindow) {
      const std::lock_guard lock(mMutex);
      mLastOwner = current;
      return;
    }
    applyForeignOwner(current, timestamp);
  }

  /** Without XFixes, notice a foreign owner by polling; the same owner copying twice stays unseen. */
  void pollOwner() {
    if (mDisplay == nullptr) {
      return;
    }
    const Window owner = XGetSelectionOwner(mDisplay, mClipboard);
    {
      const std::lock_guard lock(mMutex);
      if (owner == mLastOwner) {
        return;
      }
    }
    if (!mRunning) {
      return;
    }
    applyForeignOwner(owner, 0);
  }

  void handleSelectionRequest(const XSelectionRequestEvent& request) {
    Atom property = request.property != None ? request.property : request.target;
    const bool known = request.selection == mClipboard || request.selection == mPrimary;
    bool answered = false;
    if (known && property != None) {
      if (request.target == mTargets) {
        const std::vector<Atom> targets = advertisedTargets(request.selection);
        if (!targets.empty()) {
          XChangeProperty(mDisplay, request.requestor, property, XA_ATOM, 32, PropModeReplace,
            reinterpret_cast<const unsigned char*>(targets.data()),
            static_cast<int>(targets.size()));
          answered = true;
        }
      } else {
        const std::string mime = mimeForSelectionTarget(atomName(request.target));
        std::string data;
        bool present = false;
        if (!mime.empty()) {
          const std::lock_guard lock(mMutex);
          const ClipboardPayload& payload = payloadFor(request.selection);
          present = payload.hasFormat(mime);
          if (present) {
            data = payload.get(mime);
          }
        }
        if (present) {
          const size_t override = gIncrThresholdOverride.load(std::memory_order_relaxed);
          const size_t threshold = override != 0 ? override : mIncrThreshold;
          if (data.size() > threshold) {
            startIncrementalTransfer(request.requestor, property, request.target, std::move(data));
          } else {
            // PNG and other binary formats are transferred byte for byte: format 8 keeps NUL bytes.
            XChangeProperty(mDisplay, request.requestor, property, request.target, 8,
              PropModeReplace, reinterpret_cast<const unsigned char*>(data.data()),
              static_cast<int>(data.size()));
          }
          answered = true;
        }
      }
    }
    XEvent notify{};
    notify.xselection.type = SelectionNotify;
    notify.xselection.display = request.display;
    notify.xselection.requestor = request.requestor;
    notify.xselection.selection = request.selection;
    notify.xselection.target = request.target;
    notify.xselection.property = answered ? property : None;
    notify.xselection.time = request.time;
    XSendEvent(mDisplay, request.requestor, False, NoEventMask, &notify);
    XFlush(mDisplay);
  }

  [[nodiscard]] std::vector<Atom> advertisedTargets(const Atom selection) const {
    std::vector<std::string> mimes;
    {
      const std::lock_guard lock(mMutex);
      mimes = payloadFor(selection).formats();
    }
    std::vector<Atom> targets;
    targets.push_back(mTargets);
    for (const std::string& mime: mimes) {
      const std::string_view name = selectionTargetForMime(mime);
      if (!name.empty()) {
        targets.push_back(internTarget(name));
      }
      if (mime == "text/plain") {
        // Legacy requesters ask for the string targets directly instead of UTF8_STRING.
        targets.push_back(internTarget("TEXT"));
        targets.push_back(XA_STRING);
      }
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    return targets;
  }

  /**
   * Starts an ICCCM INCR transfer: the property is set to the `INCR` type with the total byte count,
   * and the data follows one chunk per PropertyNotify(Delete) from the requestor.
   */
  void startIncrementalTransfer(const Window requestor, const Atom property, const Atom type,
      std::string data) {
    // The requestor deletes each chunk as it reads it; those deletes are sent to the owner window.
    XSelectInput(mDisplay, requestor, PropertyChangeMask);
    const unsigned long total = static_cast<unsigned long>(data.size());
    XChangeProperty(mDisplay, requestor, property, mIncr, 32, PropModeReplace,
      reinterpret_cast<const unsigned char*>(&total), 1);
    OutgoingTransfer transfer;
    transfer.requestor = requestor;
    transfer.property = property;
    transfer.type = type;
    transfer.data = std::move(data);
    transfer.deadline = std::chrono::steady_clock::now() + kIncrTransferTimeout;
    mOutgoing.push_back(std::move(transfer));
    XFlush(mDisplay);
  }

  /** Answers a requestor's delete with the next chunk, or with a zero-length property at the end. */
  void writeIncrementalChunk(const XPropertyEvent& event) {
    if (event.state != PropertyDelete) {
      return;
    }
    size_t index = mOutgoing.size();
    for (size_t i = 0; i < mOutgoing.size(); ++i) {
      const OutgoingTransfer& transfer = mOutgoing[i];
      if (transfer.requestor == event.window && transfer.property == event.atom) {
        index = i;
        break;
      }
    }
    if (index == mOutgoing.size()) {
      return;
    }
    OutgoingTransfer& transfer = mOutgoing[index];
    if (transfer.dataComplete) {
      // The requestor consumed the last data chunk; a zero-length property is the end marker.
      XChangeProperty(mDisplay, transfer.requestor, transfer.property, transfer.type, 8,
        PropModeReplace, nullptr, 0);
      mOutgoing.erase(mOutgoing.begin() + static_cast<std::ptrdiff_t>(index));
      XFlush(mDisplay);
      return;
    }
    const size_t remaining = transfer.data.size() - transfer.offset;
    const size_t take = std::min(remaining, kIncrChunkBytes);
    const char* start = transfer.data.data() + transfer.offset;
    transfer.offset += take;
    if (take > 0) {
      XChangeProperty(mDisplay, transfer.requestor, transfer.property, transfer.type, 8,
        PropModeReplace, reinterpret_cast<const unsigned char*>(start), static_cast<int>(take));
    }
    if (transfer.offset >= transfer.data.size()) {
      // Do not append the terminator now: the requestor has to read (and delete) this last chunk
      // first, otherwise both writes collapse into one property and the tail is lost.
      transfer.dataComplete = true;
      std::string().swap(transfer.data);
    }
    XFlush(mDisplay);
  }

  /** Reads one INCR chunk and acknowledges it by deleting the property. */
  void readIncrementalChunk(const XPropertyEvent& event) {
    if (event.state != PropertyNewValue || event.window != mWindow) {
      return;
    }
    {
      const std::lock_guard lock(mMutex);
      if (!mIncrActive || mIncrProperty != event.atom) {
        return;
      }
    }
    Conversion chunk;
    const bool ok = readProperty(mDisplay, mWindow, event.atom, chunk);
    // Deleting is the acknowledgement that asks the owner for the next chunk (or ends the transfer).
    XDeleteProperty(mDisplay, mWindow, event.atom);
    XFlush(mDisplay);
    if (!ok || chunk.format != 8) {
      finishRead({});
      return;
    }
    std::string result;
    bool finished = false;
    {
      const std::lock_guard lock(mMutex);
      if (!mIncrActive || mIncrProperty != event.atom) {
        return;
      }
      if (!chunk.bytes.empty()) {
        mIncrData += chunk.bytes;
      }
      if (chunk.bytes.empty()) {
        finished = true;
        mIncrActive = false;
        result = std::move(mIncrData);
        mIncrData.clear();
      }
    }
    if (finished) {
      Conversion done;
      done.ok = true;
      done.bytes = std::move(result);
      finishRead(std::move(done));
    }
  }

  /** Forgets INCR transfers whose requestor stopped acknowledging; the connection may be gone. */
  void pruneTransfers() {
    if (mOutgoing.empty()) {
      return;
    }
    const auto now = std::chrono::steady_clock::now();
    mOutgoing.erase(std::remove_if(mOutgoing.begin(), mOutgoing.end(),
                      [now](const OutgoingTransfer& transfer) { return now > transfer.deadline; }),
      mOutgoing.end());
  }

  void finishRead(Conversion result) {
    const std::lock_guard lock(mMutex);
    mReadResult = std::move(result);
    mReadDone = true;
    mCv.notify_all();
  }

  void handleSelectionNotify(const XSelectionEvent& selection) {
    if (selection.property == None) {
      finishRead({});
      return;
    }
    // A property of type INCR is not the data: it carries the total size and starts a chunked
    // transfer. Peek at the type with a zero-length read before touching the value.
    {
      Atom type = None;
      int format = 0;
      unsigned long count = 0;
      unsigned long remaining = 0;
      unsigned char* data = nullptr;
      const int status = XGetWindowProperty(mDisplay, mWindow, selection.property, 0, 0, False,
        AnyPropertyType, &type, &format, &count, &remaining, &data);
      if (data != nullptr) {
        XFree(data);
      }
      if (status == Success && type == mIncr) {
        // Deleting the INCR property is what tells the owner to send the first chunk.
        XDeleteProperty(mDisplay, mWindow, selection.property);
        XFlush(mDisplay);
        const std::lock_guard lock(mMutex);
        mIncrActive = true;
        mIncrProperty = selection.property;
        mIncrData.clear();
        return;
      }
    }
    Conversion result;
    result.ok = readProperty(mDisplay, mWindow, selection.property, result);
    finishRead(std::move(result));
  }

  /** The owner disappeared or was replaced: our payload for that selection is no longer served. */
  void handleSelectionClear(const Atom selection) {
    if (selection == mPrimary) {
      {
        // A local set is still queued; this clear predates it.
        const std::lock_guard lock(mMutex);
        if (mPrimaryPendingOwner) {
          return;
        }
      }
      if (XGetSelectionOwner(mDisplay, mPrimary) == mWindow) {
        return;
      }
      const std::lock_guard lock(mMutex);
      mPrimaryPayload.clear();
      return;
    }
    {
      // A local set is still queued: this clear belongs to the clipboard before that set.
      const std::lock_guard lock(mMutex);
      if (mPendingOwner) {
        return;
      }
    }
    // A clear queued before our own set must not wipe the payload we already replaced; the server's
    // current owner is the tie-breaker, exactly as in handleOwnerNotification.
    if (XGetSelectionOwner(mDisplay, mClipboard) == mWindow) {
      return;
    }
    const std::lock_guard lock(mMutex);
    mOwned = false;
    mPayload.clear();
  }

  void handleEvent(XEvent& event) {
#if defined(CORAL_UI_HAS_XFIXES)
    if (mXfixesEventBase != 0 && event.type == mXfixesEventBase + XFixesSelectionNotify) {
      // Extension events are not members of the XEvent union; Xlib callers cast the generic event.
      const auto* notify = reinterpret_cast<const XFixesSelectionNotifyEvent*>(&event);
      if (notify->selection == mClipboard) {
        handleOwnerNotification(notify->timestamp);
      }
      return;
    }
#endif
    switch (event.type) {
      case SelectionRequest:
        handleSelectionRequest(event.xselectionrequest);
        break;
      case SelectionNotify:
        handleSelectionNotify(event.xselection);
        break;
      case PropertyNotify:
        // Owner side: the requestor deleted a chunk, send the next one. Requester side: a chunk
        // arrived for an INCR transfer.
        writeIncrementalChunk(event.xproperty);
        readIncrementalChunk(event.xproperty);
        break;
      case SelectionClear:
        // The XFixes/poll path reports the new owner and notifies; clearing here only drops stale data.
        handleSelectionClear(event.xselectionclear.selection);
        break;
      default:
        break;
    }
  }

  void run() {
    while (mRunning) {
      // Drain before blocking: a roundtrip (XGetSelectionOwner, XInternAtom) can read events into
      // Xlib's queue, and poll() would then sleep through its full timeout with work already pending.
      drainEvents();
      pollfd descriptors[2] = {
        { mPipe[0], POLLIN, 0 },
        { ConnectionNumber(mDisplay), POLLIN, 0 },
      };
      const int ready = poll(descriptors, 2, kPollTimeoutMs);
      if (ready < 0) {
        continue;
      }
      if ((descriptors[0].revents & POLLIN) != 0) {
        char buffer[64];
        (void)!read(mPipe[0], buffer, sizeof(buffer));
        bool readRequest = false;
        bool pendingOwner = false;
        bool pendingPrimaryOwner = false;
        {
          const std::lock_guard lock(mMutex);
          readRequest = mReadRequest;
          mReadRequest = false;
          pendingOwner = mPendingOwner;
          mPendingOwner = false;
          pendingPrimaryOwner = mPrimaryPendingOwner;
          mPrimaryPendingOwner = false;
        }
        if (pendingOwner) {
          setOwner(mClipboard);
        }
        if (pendingPrimaryOwner) {
          setOwner(mPrimary);
        }
        if (readRequest) {
          beginRead();
        }
      }
      drainEvents();
      pruneTransfers();
#if defined(CORAL_UI_HAS_XFIXES)
      if (mXfixesEventBase == 0) {
        pollOwner();
      }
#else
      pollOwner();
#endif
    }
  }

  void drainEvents() {
    while (XPending(mDisplay) > 0) {
      XEvent event{};
      XNextEvent(mDisplay, &event);
      handleEvent(event);
    }
  }

  Display* mDisplay = nullptr;
  Window mRoot = 0;
  Window mWindow = 0;
  Atom mClipboard = None;
  Atom mPrimary = None;
  Atom mTargets = None;
  Atom mProperty = None;
  Atom mIncr = None;
  int mXfixesEventBase = 0;
  int mPipe[2] = { -1, -1 };
  mutable std::mutex mMutex;
  mutable std::condition_variable mCv;
  mutable std::map<std::string, Atom> mAtoms;
  mutable ClipboardPayload mPayload;
  /** PRIMARY is a second selection with its own payload; no change notification is tracked for it. */
  mutable ClipboardPayload mPrimaryPayload;
  mutable Conversion mReadResult;
  mutable Atom mReadSelection = None;
  mutable Atom mReadTarget = None;
  mutable bool mReadDone = false;
  mutable bool mReadRequest = false;
  mutable bool mIncrActive = false;
  mutable std::string mIncrData;
  mutable Atom mIncrProperty = None;
  bool mPendingOwner = false;
  bool mPrimaryPendingOwner = false;
  bool mOwned = false;
  Window mLastOwner = None;
  Time mLastSelectionTime = 0;
  size_t mIncrThreshold = 64 * 1024;
  /** Worker-thread only; the requestor acknowledges each chunk with a PropertyNotify. */
  std::vector<OutgoingTransfer> mOutgoing;
  std::thread mThread;
  std::atomic<bool> mRunning{ false };
};

} // namespace anonymous

void setX11ClipboardIncrThresholdForTesting(const size_t bytes) {
  gIncrThresholdOverride.store(bytes, std::memory_order_relaxed);
}

std::shared_ptr<Clipboard> createX11Clipboard() {
  auto clipboard = std::make_shared<X11Clipboard>();
  if (!clipboard->valid()) {
    return nullptr;
  }
  return clipboard;
}

} // namespace coral::ui::detail
