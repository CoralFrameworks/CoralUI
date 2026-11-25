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

#include <ui/SystemFonts.h>

#include <graphics/Font.h>
#include <graphics/Typeface.h>

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>

#include <cstdio>
#include <memory>

namespace coral::ui {

std::shared_ptr<const graphics::Font> systemUIFont(const float pointSize) {
  if (pointSize <= 0.0f) return {};
  @autoreleasepool {
    NSFont* font = [NSFont systemFontOfSize:static_cast<CGFloat>(pointSize)];
    if (font == nil) return {};
    CTFontRef ctFont = (__bridge CTFontRef)font;
    CFURLRef url = (CFURLRef)CTFontCopyAttribute(ctFont, kCTFontURLAttribute);
    if (url == nullptr) return {};
    char path[4096]{};
    const bool ok = CFURLGetFileSystemRepresentation(url, true,
      reinterpret_cast<UInt8*>(path), sizeof(path));
    CFRelease(url);
    if (!ok) return {};
    std::shared_ptr<graphics::Typeface> typeface = graphics::Typeface::fromFile(path);
    if (typeface == nullptr) return {};
    return graphics::Font::create(std::move(typeface), pointSize);
  }
}

} // namespace coral::ui
