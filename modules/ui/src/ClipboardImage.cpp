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

// Clipboard image helpers.
//
// They live in their own translation unit so the image codec is pulled into a binary only by the
// programs that actually move pictures through the clipboard: a static-link consumer that never calls
// `getImage`/`setImage` does not reference this object, so it does not drag libpng/libjpeg/libwebp in
// through the clipboard.

#include <ui/Clipboard.h>

#include <graphics/Image.h>

#include <string>

namespace coral::ui {

graphics::Image Clipboard::getImage() const {
  const std::string png = get("image/png");
  if (png.empty()) return {};
  // Image::decode sniffs the container; an undecodable payload yields an empty image.
  return graphics::Image::decode(graphics::Data::makeCopy(png));
}

bool Clipboard::setImage(const graphics::Image& image) {
  if (image.isEmpty()) return false;
  const graphics::Data png = image.encode(graphics::ImageFormat::PNG, 100);
  if (png.isEmpty()) return false;
  return set("image/png", std::string(png.asText(), png.size()));
}

} // namespace coral::ui
