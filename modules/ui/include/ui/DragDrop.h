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

#ifndef CORAL_UI_DRAGDROP_H
#define CORAL_UI_DRAGDROP_H

#include <ui/Geometry.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui {

/** What a drop does with the data. A target returns the effect it accepts; `None` refuses. */
enum class DragEffect : uint8_t {
  None,
  Copy,
  Move,
  Link,
};

/**
 * `DragEffect::None` without the X11 name collision.
 *
 * Xlib defines a `None` macro (0L), so a translation unit that includes both `<X11/Xlib.h>` and this
 * header cannot spell `DragEffect::None`; use this constant there instead.
 */
constexpr DragEffect kNoDragEffect = static_cast<DragEffect>(0);

/** The data carried by one drag. One primary MIME type plus an optional secondary view. */
class DragPayload {
 public:
  DragPayload() = default;

  /** Plain text ("text/plain"). */
  [[nodiscard]] static DragPayload text(std::string text);
  /** File paths ("text/uri-list"); `paths` is the parsed list. */
  [[nodiscard]] static DragPayload files(std::vector<std::string> paths);
  /** An application-private type and its serialized data. */
  [[nodiscard]] static DragPayload custom(std::string mime, std::string data);

  [[nodiscard]] const std::string& type() const noexcept {
    return mType;
  }
  [[nodiscard]] const std::string& data() const noexcept {
    return mData;
  }
  [[nodiscard]] const std::vector<std::string>& paths() const noexcept {
    return mPaths;
  }
  [[nodiscard]] bool empty() const noexcept {
    return mType.empty();
  }
  /** True when this payload can be read as @p mime (the primary type matches). */
  [[nodiscard]] bool hasType(std::string_view mime) const {
    return mType == mime;
  }

  /** Attaches a second representation of the same data ("text/plain" next to a private type). */
  void setSecondary(std::string mime, std::string data);
  [[nodiscard]] bool hasSecondary() const noexcept {
    return !mSecondaryType.empty();
  }
  [[nodiscard]] const std::string& secondaryType() const noexcept {
    return mSecondaryType;
  }
  [[nodiscard]] const std::string& secondaryData() const noexcept {
    return mSecondaryData;
  }

 private:
  std::string mType;
  std::string mData;
  std::vector<std::string> mPaths;
  std::string mSecondaryType;
  std::string mSecondaryData;
};

/**
 * A drag in progress, as seen by the layer under the pointer.
 *
 * `position` is in logical window units. `effect` is filled by the target for a drop; for
 * enter/over the target returns the effect instead.
 */
struct DragEvent {
  Point position{};
  DragPayload payload{};
  DragEffect effect = DragEffect::None;
  /** True when the data comes from outside the application (OS drag, file manager). */
  bool external = false;
};

} // namespace coral::ui

#endif // CORAL_UI_DRAGDROP_H
