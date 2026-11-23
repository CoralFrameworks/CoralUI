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

/**
 * @file SystemImageCocoa.mm
 *
 * SF Symbols as ordinary Coral images. AppKit owns the symbol artwork; this file rasterises the
 * configured NSImage into a bitmap at the requested scale and decodes the PNG with the graphics
 * image decoder, so the rest of CoralUI needs no AppKit types.
 */

#include <ui/SystemImage.h>

#include <graphics/Data.h>

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace coral::ui {

graphics::Image systemSymbolImage(const std::string_view name, const SystemImageOptions& options) {
  if (name.empty()) return {};
  @autoreleasepool {
    if (@available(macOS 11.0, *)) {
      NSString* symbolName = [NSString stringWithUTF8String:std::string(name).c_str()];
      NSString* description = options.description.empty()
        ? nil : [NSString stringWithUTF8String:options.description.c_str()];
      NSImage* symbol = [NSImage imageWithSystemSymbolName:symbolName
                                 accessibilityDescription:description];
      if (symbol == nil) return {};
      NSImageSymbolConfiguration* configuration = [NSImageSymbolConfiguration
        configurationWithPointSize:options.pointSize weight:NSFontWeightRegular];
      NSImage* configured = [symbol imageWithSymbolConfiguration:configuration];
      if (configured == nil) configured = symbol;
      [configured setTemplate:YES];
      const NSSize pointSize = configured.size;
      if (pointSize.width <= 0.0 || pointSize.height <= 0.0) return {};
      const CGFloat scale = options.scale > 0.0f ? static_cast<CGFloat>(options.scale) : 2.0;
      const NSInteger width = std::max<NSInteger>(1, std::lround(pointSize.width * scale));
      const NSInteger height = std::max<NSInteger>(1, std::lround(pointSize.height * scale));
      NSBitmapImageRep* bitmap = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nullptr pixelsWide:width pixelsHigh:height bitsPerSample:8
                   samplesPerPixel:4 hasAlpha:YES isPlanar:NO
                   colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
      if (bitmap == nil) return {};
      NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
      if (context == nil) return {};
      [NSGraphicsContext saveGraphicsState];
      [NSGraphicsContext setCurrentContext:context];
      [configured drawInRect:NSMakeRect(0.0, 0.0, width, height)
                    fromRect:NSZeroRect
                   operation:NSCompositingOperationSourceOver
                    fraction:1.0];
      [NSGraphicsContext restoreGraphicsState];
      NSData* png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
      if (png == nil || png.length == 0) return {};
      return graphics::Image::decode(
        graphics::Data::makeCopy(png.bytes, static_cast<size_t>(png.length)));
    }
  }
  return {};
}

bool supportsSystemSymbols() noexcept {
  if (@available(macOS 11.0, *)) return YES;
  return NO;
}

} // namespace coral::ui
