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

#ifndef CORAL_UI_SYSTEMIMAGE_H
#define CORAL_UI_SYSTEMIMAGE_H

#include <graphics/Image.h>

#include <string>
#include <string_view>

namespace coral::ui {

/** How a platform system image is resolved. */
struct SystemImageOptions {
  /** Requested point size. */
  float pointSize = 16.0f;
  /** Rasterisation scale; 0 uses 2x so the image is usable on Retina displays. */
  float scale = 0.0f;
  /** Optional description passed to the platform's accessibility metadata. */
  std::string description;
};

/**
 * Loads a platform system image (SF Symbols on macOS) as a regular `graphics::Image`.
 *
 * On macOS this is `NSImage imageWithSystemSymbolName:accessibilityDescription:`, rendered into a
 * bitmap at the requested scale. The result can be drawn by `ImageLayer`/`IconButton` like any other
 * image. A platform without system images returns an empty image.
 */
[[nodiscard]] graphics::Image systemSymbolImage(std::string_view name,
  const SystemImageOptions& options = {});

/** @return true when the current platform can resolve symbol names. */
[[nodiscard]] bool supportsSystemSymbols() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_SYSTEMIMAGE_H
