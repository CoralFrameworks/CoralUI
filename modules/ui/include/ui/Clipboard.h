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

#ifndef CORAL_UI_CLIPBOARD_H
#define CORAL_UI_CLIPBOARD_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace graphics {
class Image;
}

namespace coral::ui {

/**
 * A multi-format clipboard.
 *
 * Formats are MIME types ("text/plain", "text/uri-list", "text/html", "image/png", or an
 * application-private type). `get`/`set` are synchronous because `TextInput` needs the data in the
 * same event; a backend that can only read asynchronously (Wayland's data device) waits internally,
 * which is what the current Linux backends do.
 *
 * `getText`/`setText` are the compatibility wrappers around "text/plain"; new code should pass the
 * MIME type so a copy can carry the richest representation it has (a target picks the type it knows)
 * instead of flattening everything to text.
 */
class Clipboard {
 public:
  /** Called after the content changed (a local `set`, or a platform notification). */
  using ChangeHandler = std::function<void()>;

  virtual ~Clipboard() = default;

  /** Reads @p mime; an empty string when the format is absent. */
  [[nodiscard]] virtual std::string get(const std::string& mime) const = 0;
  /**
   * Stores one representation for @p mime; whether earlier formats survive is backend-defined.
   *
   * A selection-owner backend (X11 CLIPBOARD, Wayland data source) and `MemoryClipboard` keep the
   * other formats, so a copy can publish text plus `text/html` plus `image/png`; the Win32
   * pasteboard is replaced on every write. @return false when the backend cannot produce @p mime.
   */
  virtual bool set(const std::string& mime, std::string data) = 0;
  /** The formats currently available; the order is backend-defined. */
  [[nodiscard]] virtual std::vector<std::string> formats() const = 0;

  [[nodiscard]] bool hasFormat(const std::string& mime) const {
    const std::vector<std::string> available = formats();
    return std::find(available.begin(), available.end(), mime) != available.end();
  }

  [[nodiscard]] std::string getText() const {
    return get("text/plain");
  }
  void setText(std::string text) {
    (void) set("text/plain", std::move(text));
  }

  /** `text/uri-list` helpers: one path per line, comments ignored. */
  [[nodiscard]] std::vector<std::string> getFiles() const;
  bool setFiles(const std::vector<std::string>& paths);

  /**
   * Decodes the `image/png` representation, or returns an empty image when it is absent or undecodable.
   *
   * Pictures travel as PNG because every platform backend carries that type, so a caller can paste a
   * screenshot without knowing the native format. Like `getFiles` this is a convenience on top of
   * `get("image/png")`, not a separate backend capability.
   */
  [[nodiscard]] graphics::Image getImage() const;
  /** Encodes @p image as PNG; false when it is empty or the backend cannot store `image/png`. */
  bool setImage(const graphics::Image& image);

  /**
   * Reads the PRIMARY selection ("select to copy") for @p mime, or "" where the platform has none.
   *
   * X11 and Wayland keep a second selection that follows the mouse: marking text publishes it and a
   * middle click pastes it. Windows has no equivalent, so the default implementation reports absence.
   * An empty string can also mean an empty PRIMARY; `primaryFormats` tells the two apart.
   */
  [[nodiscard]] virtual std::string getPrimary(const std::string& mime) const;

  /**
   * Replaces the PRIMARY selection for @p mime; false where the platform has none.
   *
   * This is not the clipboard: it does not fire the change handler and `Clipboard::systemChangeCount`
   * does not move, because Linux applications expect the primary selection to change silently while
   * the user drags.
   */
  virtual bool setPrimary(const std::string& mime, std::string data);

  /** The MIME types currently in PRIMARY; empty when the platform has none. */
  [[nodiscard]] virtual std::vector<std::string> primaryFormats() const;

  /**
   * Observes clipboard changes.
   *
   * The handler runs on whichever thread noticed the change; UI code should marshal it with
   * `UiExecutor`. A backend without platform notification only fires it for local `set` calls.
   */
  void setChangeHandler(ChangeHandler handler);
  [[nodiscard]] bool hasChangeHandler() const;

  /** The process-wide clipboard; lazily creates the platform backend on first use. */
  [[nodiscard]] static std::shared_ptr<Clipboard> system();

  /**
   * Replaces the process-wide clipboard.
   *
   * Tests use this to install a `MemoryClipboard`; an application can do the same on a platform
   * whose native backend is not implemented yet.
   */
  static void setSystem(std::shared_ptr<Clipboard> clipboard);

  /**
   * Announces a platform clipboard change to the process-wide clipboard's handler.
   *
   * A platform backend calls this from its own change notification (Win32 `WM_CLIPBOARDUPDATE` is
   * wired this way); the handler then runs on the thread that noticed the change.
   */
  static void notifySystemChanged();

  /**
   * A monotonic counter of platform clipboard changes, or 0 when the platform has no such notion.
   *
   * This is the pull side of change detection, for a UI that already polls (a drag hover loop, a
   * toolbar refresh): compare two reads and call `notifySystemChanged()` yourself, or rely on the
   * platform's push notification where it exists (Win32 `WM_CLIPBOARDUPDATE`). It counts changes
   * made by any process, including this one.
   */
  [[nodiscard]] static uint64_t systemChangeCount();

 protected:
  void notifyChanged() const;

 private:
  mutable std::mutex mHandlerMutex;
  ChangeHandler mChangeHandler;
};

/**
 * A process-local clipboard.
 *
 * It is the fallback backend on platforms without a native implementation, and the deterministic
 * backend the tests use. The mutex makes it safe if a model thread reads it through an executor.
 */
class MemoryClipboard final : public Clipboard {
 public:
  [[nodiscard]] std::string get(const std::string& mime) const override;
  bool set(const std::string& mime, std::string data) override;
  [[nodiscard]] std::vector<std::string> formats() const override;

  [[nodiscard]] std::string getPrimary(const std::string& mime) const override;
  bool setPrimary(const std::string& mime, std::string data) override;
  [[nodiscard]] std::vector<std::string> primaryFormats() const override;

 private:
  mutable std::mutex mMutex;
  std::vector<std::pair<std::string, std::string>> mFormats;
  std::vector<std::pair<std::string, std::string>> mPrimaryFormats;
};

} // namespace coral::ui

#endif // CORAL_UI_CLIPBOARD_H
