/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_ICONTHEME_H
#define CORAL_UI_ICONTHEME_H

#include <ui/Types.h>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace coral::ui {

/** One resolution of an icon; `scale` is the asset's pixel density (1.0, 2.0, ...). */
struct IconVariant {
  std::shared_ptr<const Image> image;
  float scale = 1.0f;
};

/**
 * A name-addressed, multi-resolution icon pack.
 *
 * A theme is shared by controls and owns no window state. `resolve()` chooses the variant whose
 * intrinsic size (image pixels divided by `scale`) is closest to the requested logical size,
 * preferring the smallest variant that is at least as large as the request.
 */
class IconTheme {
 public:
  void add(std::string name, std::shared_ptr<const Image> image, float scale = 1.0f);
  void add(std::string name, std::vector<IconVariant> variants);
  [[nodiscard]] bool contains(std::string_view name) const;
  [[nodiscard]] const Image* resolve(std::string_view name, float logicalSize) const;
  [[nodiscard]] const std::vector<std::string>& names() const noexcept { return mNames; }

 private:
  struct Entry {
    std::vector<IconVariant> variants;
  };
  std::unordered_map<std::string, Entry> mEntries;
  std::vector<std::string> mNames;
};

} // namespace coral::ui

#endif // CORAL_UI_ICONTHEME_H
