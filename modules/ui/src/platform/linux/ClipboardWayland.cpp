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

#include <wayland-client.h>
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
#include "primary-selection-unstable-v1-client-protocol.h"
#endif

#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace coral::ui::detail {

namespace {

constexpr const char* kUtf8Mime = "text/plain;charset=utf-8";
constexpr const char* kPlainMime = "text/plain";
std::atomic<uint32_t> gInputSerial{ 0 };

/** Maps a Wayland offer MIME to the clipboard MIME type it represents. */
std::string mimeForWaylandOffer(const std::string_view offer) {
  if (offer == kUtf8Mime || offer == kPlainMime || offer == "UTF8_STRING" || offer == "STRING" ||
      offer == "TEXT") {
    return "text/plain";
  }
  if (offer.find('/') != std::string_view::npos) {
    return std::string(offer);
  }
  return {};
}

void writeAll(const int fd, const std::string& data) {
  size_t written = 0;
  while (written < data.size()) {
    const ssize_t count = ::write(fd, data.data() + written, data.size() - written);
    if (count > 0) {
      written += static_cast<size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
}

/**
 * Wayland clipboard backed by `wl_data_device` and a private event connection.
 *
 * A selection owner has to keep serving `wl_data_source.send` for as long as it owns the selection,
 * so the connection lives on a worker thread. The public API remains synchronous through a pipe and
 * a condition variable. The serial used for `set_selection` is the last keyboard/pointer serial the
 * compositor has given this client; 0 is a best-effort value for a clipboard set before any input.
 *
 * Formats are MIME types, and Wayland's data source/offer exchange is already per-MIME, so an owner
 * offers every representation it holds and `send` answers with the bytes for the requested MIME.
 * This is what makes `text/html`, `text/uri-list` and `image/png` (NUL bytes included) work next to
 * plain text instead of flattening the clipboard to one string.
 */
class WaylandClipboard final : public Clipboard {
 public:
  WaylandClipboard() {
    mDisplay = wl_display_connect(nullptr);
    if (mDisplay == nullptr) return;
    mRegistry = wl_display_get_registry(mDisplay);
    wl_registry_add_listener(mRegistry, &kRegistryListener, this);
    wl_display_roundtrip(mDisplay);
    if (mManager == nullptr || mSeat == nullptr) {
      wl_display_disconnect(mDisplay);
      mDisplay = nullptr;
      return;
    }
    mDevice = wl_data_device_manager_get_data_device(mManager, mSeat);
    wl_data_device_add_listener(mDevice, &kDeviceListener, this);
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
    if (mPrimaryManager != nullptr) {
      mPrimaryDevice = zwp_primary_selection_device_manager_v1_get_device(mPrimaryManager, mSeat);
      if (mPrimaryDevice != nullptr) {
        zwp_primary_selection_device_v1_add_listener(mPrimaryDevice, &kPrimaryDeviceListener, this);
      }
    }
#endif
    if (pipe(mPipe) != 0) {
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
      if (mPrimaryDevice != nullptr) zwp_primary_selection_device_v1_destroy(mPrimaryDevice);
      if (mPrimaryManager != nullptr) zwp_primary_selection_device_manager_v1_destroy(mPrimaryManager);
#endif
      wl_data_device_destroy(mDevice);
      wl_data_device_manager_destroy(mManager);
      wl_seat_destroy(mSeat);
      wl_registry_destroy(mRegistry);
      wl_display_disconnect(mDisplay);
      mDisplay = nullptr;
      return;
    }
    mRunning = true;
    mThread = std::thread([this] { run(); });
  }

  ~WaylandClipboard() override {
    if (mDisplay == nullptr) return;
    mRunning = false;
    wake();
    if (mThread.joinable()) mThread.join();
    close(mPipe[0]);
    close(mPipe[1]);
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
    if (mPrimarySource != nullptr) zwp_primary_selection_source_v1_destroy(mPrimarySource);
    if (mPrimaryOffer != nullptr) zwp_primary_selection_offer_v1_destroy(mPrimaryOffer);
    if (mPrimaryDevice != nullptr) zwp_primary_selection_device_v1_destroy(mPrimaryDevice);
    if (mPrimaryManager != nullptr) zwp_primary_selection_device_manager_v1_destroy(mPrimaryManager);
#endif
    if (mSource != nullptr) wl_data_source_destroy(mSource);
    if (mOffer != nullptr) wl_data_offer_destroy(mOffer);
    if (mDevice != nullptr) wl_data_device_destroy(mDevice);
    if (mManager != nullptr) wl_data_device_manager_destroy(mManager);
    if (mSeat != nullptr) wl_seat_destroy(mSeat);
    if (mRegistry != nullptr) wl_registry_destroy(mRegistry);
    wl_display_disconnect(mDisplay);
  }

  [[nodiscard]] bool valid() const noexcept {
    return mDisplay != nullptr;
  }

  [[nodiscard]] std::string get(const std::string& mime) const override {
    if (mDisplay == nullptr || mime.empty()) return {};
    {
      const std::lock_guard lock(mMutex);
      if (mOwned) {
        return mPayload.get(mime);
      }
      // Asking for a MIME the source does not advertise would wait for data that never arrives.
      if (resolveOfferMimeLocked(mime).empty()) return {};
      mReadPrimary = false;
      mReadMime = mime;
      mReadResult.clear();
      mReadDone = false;
      mReadRequest = true;
    }
    wake();
    std::unique_lock lock(mMutex);
    if (!mCv.wait_for(lock, std::chrono::seconds(1), [this] { return mReadDone; })) {
      return {};
    }
    return mReadResult;
  }

  bool set(const std::string& mime, std::string data) override {
    if (mDisplay == nullptr || mime.empty() || mime.find('/') == std::string::npos) return false;
    {
      const std::lock_guard lock(mMutex);
      mPayload.set(mime, std::move(data));
      mOwned = true;
      mPendingOwner = true;
    }
    notePlatformClipboardChanged();
    notifyChanged();
    wake();
    return true;
  }

  [[nodiscard]] std::vector<std::string> formats() const override {
    const std::lock_guard lock(mMutex);
    if (mOwned) {
      return mPayload.formats();
    }
    std::vector<std::string> result;
    for (const std::string& offer: mOfferMimes) {
      const std::string mime = mimeForWaylandOffer(offer);
      if (mime.empty()) continue;
      if (std::find(result.begin(), result.end(), mime) == result.end()) {
        result.push_back(mime);
      }
    }
    return result;
  }

#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
  [[nodiscard]] std::string getPrimary(const std::string& mime) const override {
    if (mDisplay == nullptr || mPrimaryManager == nullptr || mPrimaryDevice == nullptr ||
        mime.empty()) {
      return {};
    }
    {
      const std::lock_guard lock(mMutex);
      if (mPrimaryOwned) {
        return mPrimaryPayload.get(mime);
      }
      if (resolvePrimaryOfferMimeLocked(mime).empty()) return {};
      mReadPrimary = true;
      mReadMime = mime;
      mReadResult.clear();
      mReadDone = false;
      mReadRequest = true;
    }
    wake();
    std::unique_lock lock(mMutex);
    if (!mCv.wait_for(lock, std::chrono::seconds(1), [this] { return mReadDone; })) {
      return {};
    }
    return mReadResult;
  }

  bool setPrimary(const std::string& mime, std::string data) override {
    if (mDisplay == nullptr || mPrimaryManager == nullptr || mPrimaryDevice == nullptr ||
        mime.empty() || mime.find('/') == std::string::npos) {
      return false;
    }
    {
      const std::lock_guard lock(mMutex);
      mPrimaryPayload.set(mime, std::move(data));
      mPrimaryOwned = true;
      mPrimaryPendingOwner = true;
    }
    // PRIMARY is not the clipboard: no change notification.
    wake();
    return true;
  }

  [[nodiscard]] std::vector<std::string> primaryFormats() const override {
    if (mDisplay == nullptr || mPrimaryManager == nullptr) {
      return {};
    }
    const std::lock_guard lock(mMutex);
    if (mPrimaryOwned) {
      return mPrimaryPayload.formats();
    }
    std::vector<std::string> result;
    for (const std::string& offer: mPrimaryOfferMimes) {
      const std::string mime = mimeForWaylandOffer(offer);
      if (mime.empty()) continue;
      if (std::find(result.begin(), result.end(), mime) == result.end()) {
        result.push_back(mime);
      }
    }
    return result;
  }
#endif

 private:
  /**
   * The offer MIME to request for @p mime, preferring the UTF-8 text flavour when the source offers
   * several. Must be called with `mMutex` held.
   */
  [[nodiscard]] std::string resolveOfferMimeLocked(const std::string& mime) const {
    if (mOffer == nullptr) return {};
    const auto offered = [this](const std::string& name) {
      return std::find(mOfferMimes.begin(), mOfferMimes.end(), name) != mOfferMimes.end();
    };
    if (offered(mime)) return mime;
    if (mime == "text/plain") {
      for (const char* alias: { kUtf8Mime, kPlainMime, "UTF8_STRING", "STRING" }) {
        if (offered(alias)) return alias;
      }
    }
    return {};
  }

#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
  /** Same resolution for the PRIMARY offer; must be called with `mMutex` held. */
  [[nodiscard]] std::string resolvePrimaryOfferMimeLocked(const std::string& mime) const {
    if (mPrimaryOffer == nullptr) return {};
    const auto offered = [this](const std::string& name) {
      return std::find(mPrimaryOfferMimes.begin(), mPrimaryOfferMimes.end(), name) !=
        mPrimaryOfferMimes.end();
    };
    if (offered(mime)) return mime;
    if (mime == "text/plain") {
      for (const char* alias: { kUtf8Mime, kPlainMime, "UTF8_STRING", "STRING" }) {
        if (offered(alias)) return alias;
      }
    }
    return {};
  }
#endif

  void wake() const {
    if (mPipe[1] >= 0) {
      const char byte = 1;
      (void)!write(mPipe[1], &byte, 1);
    }
  }

  /** The MIME names a data source advertises for @p payload (caller holds the mutex). */
  [[nodiscard]] static std::vector<std::string> sourceOffers(const ClipboardPayload& payload) {
    std::vector<std::string> offers;
    for (const std::string& mime: payload.formats()) {
      if (mime == "text/plain") {
        offers.emplace_back(kUtf8Mime);
        offers.emplace_back(kPlainMime);
        // Legacy string names some toolkits still ask for; harmless for the others.
        offers.emplace_back("UTF8_STRING");
        offers.emplace_back("STRING");
      } else {
        offers.push_back(mime);
      }
    }
    return offers;
  }

  void setSelection() {
    if (mManager == nullptr || mDevice == nullptr) return;
    if (mSource != nullptr) {
      wl_data_source_destroy(mSource);
      mSource = nullptr;
    }
    std::vector<std::string> offers;
    {
      const std::lock_guard lock(mMutex);
      offers = sourceOffers(mPayload);
    }
    mSource = wl_data_device_manager_create_data_source(mManager);
    if (mSource == nullptr) return;
    wl_data_source_add_listener(mSource, &kSourceListener, this);
    for (const std::string& offer: offers) {
      wl_data_source_offer(mSource, offer.c_str());
    }
    wl_data_device_set_selection(mDevice, mSource, gInputSerial.load());
    wl_display_flush(mDisplay);
  }

#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
  void setPrimarySelection() {
    if (mPrimaryManager == nullptr || mPrimaryDevice == nullptr) return;
    if (mPrimarySource != nullptr) {
      zwp_primary_selection_source_v1_destroy(mPrimarySource);
      mPrimarySource = nullptr;
    }
    std::vector<std::string> offers;
    {
      const std::lock_guard lock(mMutex);
      offers = sourceOffers(mPrimaryPayload);
    }
    mPrimarySource = zwp_primary_selection_device_manager_v1_create_source(mPrimaryManager);
    if (mPrimarySource == nullptr) return;
    zwp_primary_selection_source_v1_add_listener(mPrimarySource, &kPrimarySourceListener, this);
    for (const std::string& offer: offers) {
      zwp_primary_selection_source_v1_offer(mPrimarySource, offer.c_str());
    }
    zwp_primary_selection_device_v1_set_selection(mPrimaryDevice, mPrimarySource,
      gInputSerial.load());
    wl_display_flush(mDisplay);
  }
#endif

  void readSelection() {
    wl_data_offer* offer = nullptr;
    std::string offeredMime;
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
    zwp_primary_selection_offer_v1* primaryOffer = nullptr;
    bool primary = false;
#endif
    {
      const std::lock_guard lock(mMutex);
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
      primary = mReadPrimary;
      if (primary) {
        primaryOffer = mPrimaryOffer;
        offeredMime = resolvePrimaryOfferMimeLocked(mReadMime);
      } else
#endif
      {
        offer = mOffer;
        offeredMime = resolveOfferMimeLocked(mReadMime);
      }
    }
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
    const bool available = primary ? primaryOffer != nullptr : offer != nullptr;
#else
    const bool available = offer != nullptr;
#endif
    if (!available || offeredMime.empty()) {
      finishRead({});
      return;
    }
    int input[2] = { -1, -1 };
    if (pipe(input) != 0) {
      finishRead({});
      return;
    }
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
    if (primary) {
      zwp_primary_selection_offer_v1_receive(primaryOffer, offeredMime.c_str(), input[1]);
    } else
#endif
    {
      wl_data_offer_receive(offer, offeredMime.c_str(), input[1]);
    }
    // The compositor writes into the read end; the write end must be closed for read() to see EOF.
    close(input[1]);
    wl_display_flush(mDisplay);
    std::string result;
    char buffer[8192];
    for (;;) {
      const ssize_t count = read(input[0], buffer, sizeof(buffer));
      if (count > 0) {
        result.append(buffer, static_cast<size_t>(count));
        continue;
      }
      if (count < 0 && errno == EINTR) {
        continue;
      }
      break;
    }
    close(input[0]);
    finishRead(std::move(result));
  }

  void finishRead(std::string result) {
    const std::lock_guard lock(mMutex);
    mReadResult = std::move(result);
    mReadDone = true;
    mCv.notify_all();
  }

  void run() {
    while (mRunning) {
      pollfd descriptors[2] = {
        { mPipe[0], POLLIN, 0 },
        { wl_display_get_fd(mDisplay), POLLIN, 0 },
      };
      const int ready = poll(descriptors, 2, 100);
      if (ready < 0) continue;
      if ((descriptors[0].revents & POLLIN) != 0) {
        char buffer[64];
        (void)!read(mPipe[0], buffer, sizeof(buffer));
        bool pendingOwner = false;
        bool readRequest = false;
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
        bool pendingPrimaryOwner = false;
#endif
        {
          const std::lock_guard lock(mMutex);
          pendingOwner = mPendingOwner;
          mPendingOwner = false;
          readRequest = mReadRequest;
          mReadRequest = false;
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
          pendingPrimaryOwner = mPrimaryPendingOwner;
          mPrimaryPendingOwner = false;
#endif
        }
        if (pendingOwner) setSelection();
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
        if (pendingPrimaryOwner) setPrimarySelection();
#endif
        if (readRequest) readSelection();
      }
      if ((descriptors[1].revents & POLLIN) != 0) {
        while (wl_display_prepare_read(mDisplay) != 0) {
          wl_display_dispatch_pending(mDisplay);
        }
        wl_display_read_events(mDisplay);
        wl_display_dispatch_pending(mDisplay);
      } else {
        wl_display_dispatch_pending(mDisplay);
      }
    }
  }

  wl_display* mDisplay = nullptr;
  wl_registry* mRegistry = nullptr;
  wl_seat* mSeat = nullptr;
  wl_data_device_manager* mManager = nullptr;
  wl_data_device* mDevice = nullptr;
  wl_data_source* mSource = nullptr;
  wl_data_offer* mOffer = nullptr;
  std::vector<std::string> mOfferMimes;
  int mPipe[2] = { -1, -1 };
  mutable std::mutex mMutex;
  mutable std::condition_variable mCv;
  ClipboardPayload mPayload;
  mutable std::string mReadResult;
  mutable std::string mReadMime;
  mutable bool mReadDone = false;
  mutable bool mReadRequest = false;
  mutable bool mReadPrimary = false;
  bool mPendingOwner = false;
  bool mOwned = false;
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
  zwp_primary_selection_device_manager_v1* mPrimaryManager = nullptr;
  zwp_primary_selection_device_v1* mPrimaryDevice = nullptr;
  zwp_primary_selection_source_v1* mPrimarySource = nullptr;
  zwp_primary_selection_offer_v1* mPrimaryOffer = nullptr;
  std::vector<std::string> mPrimaryOfferMimes;
  ClipboardPayload mPrimaryPayload;
  bool mPrimaryPendingOwner = false;
  bool mPrimaryOwned = false;
#endif
  std::thread mThread;
  std::atomic<bool> mRunning{ false };

  static void registryGlobal(void* data, wl_registry* registry, uint32_t name,
      const char* interface, uint32_t version) {
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    if (std::strcmp(interface, wl_seat_interface.name) == 0 && clipboard->mSeat == nullptr) {
      clipboard->mSeat = static_cast<wl_seat*>(
        wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 5u)));
    } else if (std::strcmp(interface, wl_data_device_manager_interface.name) == 0 &&
        clipboard->mManager == nullptr) {
      clipboard->mManager = static_cast<wl_data_device_manager*>(
        wl_registry_bind(registry, name, &wl_data_device_manager_interface, std::min(version, 3u)));
#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
    } else if (std::strcmp(interface, zwp_primary_selection_device_manager_v1_interface.name) == 0 &&
        clipboard->mPrimaryManager == nullptr) {
      // Version 1 is all the protocol has; the composer may simply not advertise it.
      clipboard->mPrimaryManager = static_cast<zwp_primary_selection_device_manager_v1*>(
        wl_registry_bind(registry, name, &zwp_primary_selection_device_manager_v1_interface, 1u));
#endif
    }
  }
  static const wl_registry_listener kRegistryListener;

  static void offerMime(void* data, wl_data_offer*, const char* mime) {
    if (mime == nullptr) return;
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    const std::lock_guard lock(clipboard->mMutex);
    clipboard->mOfferMimes.emplace_back(mime);
  }
  static const wl_data_offer_listener kOfferListener;

  static void selection(void* data, wl_data_device*, wl_data_offer* offer) {
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    bool foreign = false;
    {
      const std::lock_guard lock(clipboard->mMutex);
      if (clipboard->mOffer != nullptr) {
        wl_data_offer_destroy(clipboard->mOffer);
        clipboard->mOffer = nullptr;
      }
      clipboard->mOfferMimes.clear();
      clipboard->mOffer = offer;
      // While we own the selection the compositor may echo it back; our own write is not a change.
      foreign = !clipboard->mOwned;
    }
    if (foreign) {
      notePlatformClipboardChanged();
      clipboard->notifyChanged();
    }
  }
  static const wl_data_device_listener kDeviceListener;

  static void sourceSend(void* data, wl_data_source*, const char* mime, int32_t fd) {
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    std::string payload;
    if (mime != nullptr) {
      const std::string wanted = mimeForWaylandOffer(mime);
      const std::lock_guard lock(clipboard->mMutex);
      payload = clipboard->mPayload.get(wanted);
    }
    writeAll(fd, payload);
    close(fd);
  }

  static void sourceCancelled(void* data, wl_data_source* source) {
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    bool wasOwned = false;
    {
      const std::lock_guard lock(clipboard->mMutex);
      wasOwned = clipboard->mOwned;
      clipboard->mOwned = false;
      clipboard->mPayload.clear();
      if (clipboard->mSource == source) {
        clipboard->mSource = nullptr;
      }
    }
    wl_data_source_destroy(source);
    if (wasOwned) {
      // Another client took the clipboard; the payload we were serving is gone.
      notePlatformClipboardChanged();
      clipboard->notifyChanged();
    }
  }
  static const wl_data_source_listener kSourceListener;

#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
  static void primaryOfferMime(void* data, zwp_primary_selection_offer_v1*, const char* mime) {
    if (mime == nullptr) return;
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    const std::lock_guard lock(clipboard->mMutex);
    clipboard->mPrimaryOfferMimes.emplace_back(mime);
  }
  static const zwp_primary_selection_offer_v1_listener kPrimaryOfferListener;

  static void primaryDeviceOffer(void* data, zwp_primary_selection_device_v1*,
      zwp_primary_selection_offer_v1* offer) {
    if (offer == nullptr) return;
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    zwp_primary_selection_offer_v1_add_listener(offer, &kPrimaryOfferListener, clipboard);
  }

  static void primarySelection(void* data, zwp_primary_selection_device_v1*,
      zwp_primary_selection_offer_v1* offer) {
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    const std::lock_guard lock(clipboard->mMutex);
    if (clipboard->mPrimaryOffer != nullptr) {
      zwp_primary_selection_offer_v1_destroy(clipboard->mPrimaryOffer);
      clipboard->mPrimaryOffer = nullptr;
    }
    clipboard->mPrimaryOfferMimes.clear();
    clipboard->mPrimaryOffer = offer;
  }
  static const zwp_primary_selection_device_v1_listener kPrimaryDeviceListener;

  static void primarySourceSend(void* data, zwp_primary_selection_source_v1*, const char* mime,
      int32_t fd) {
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    std::string payload;
    if (mime != nullptr) {
      const std::string wanted = mimeForWaylandOffer(mime);
      const std::lock_guard lock(clipboard->mMutex);
      payload = clipboard->mPrimaryPayload.get(wanted);
    }
    writeAll(fd, payload);
    close(fd);
  }

  static void primarySourceCancelled(void* data, zwp_primary_selection_source_v1* source) {
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    {
      const std::lock_guard lock(clipboard->mMutex);
      clipboard->mPrimaryOwned = false;
      clipboard->mPrimaryPayload.clear();
      if (clipboard->mPrimarySource == source) {
        clipboard->mPrimarySource = nullptr;
      }
    }
    zwp_primary_selection_source_v1_destroy(source);
    // PRIMARY changes are silent by design: no clipboard notification and no change counter bump.
  }
  static const zwp_primary_selection_source_v1_listener kPrimarySourceListener;
#endif
};

const wl_registry_listener WaylandClipboard::kRegistryListener = {
  WaylandClipboard::registryGlobal,
  [](void*, wl_registry*, uint32_t) {},
};

const wl_data_offer_listener WaylandClipboard::kOfferListener = {
  WaylandClipboard::offerMime,
  [](void*, wl_data_offer*, uint32_t) {},
  [](void*, wl_data_offer*, uint32_t) {},
};

const wl_data_device_listener WaylandClipboard::kDeviceListener = {
  [](void* data, wl_data_device*, wl_data_offer* offer) {
    auto* clipboard = static_cast<WaylandClipboard*>(data);
    if (offer == nullptr) return;
    wl_data_offer_add_listener(offer, &kOfferListener, clipboard);
  },
  [](void*, wl_data_device*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t, wl_data_offer*) {},
  [](void*, wl_data_device*) {},
  [](void*, wl_data_device*, uint32_t, wl_fixed_t, wl_fixed_t) {},
  [](void*, wl_data_device*) {},
  WaylandClipboard::selection,
};

const wl_data_source_listener WaylandClipboard::kSourceListener = {
  [](void*, wl_data_source*, const char*) {},
  WaylandClipboard::sourceSend,
  WaylandClipboard::sourceCancelled,
  [](void*, wl_data_source*) {},
  [](void*, wl_data_source*) {},
  [](void*, wl_data_source*, uint32_t) {},
};

#if defined(CORAL_UI_WAYLAND_PRIMARY_SELECTION)
const zwp_primary_selection_offer_v1_listener WaylandClipboard::kPrimaryOfferListener = {
  WaylandClipboard::primaryOfferMime,
};

const zwp_primary_selection_device_v1_listener WaylandClipboard::kPrimaryDeviceListener = {
  WaylandClipboard::primaryDeviceOffer,
  WaylandClipboard::primarySelection,
};

const zwp_primary_selection_source_v1_listener WaylandClipboard::kPrimarySourceListener = {
  WaylandClipboard::primarySourceSend,
  WaylandClipboard::primarySourceCancelled,
};
#endif

} // namespace

void setWaylandClipboardSerial(const uint32_t serial) {
  gInputSerial.store(serial);
}

std::shared_ptr<Clipboard> createWaylandClipboard() {
  auto clipboard = std::make_shared<WaylandClipboard>();
  if (!clipboard->valid()) return nullptr;
  return clipboard;
}

} // namespace coral::ui::detail
