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

#include <ui/macos/NativeColorWell.h>

#include "NativeColorWellInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <utility>

char kNativeColorWellTargetKey;

graphics::Color colorFromNative(NSColor* color) {
  NSColor* srgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
  if (srgb == nil) srgb = color;
  return graphics::Color(static_cast<float>(srgb.redComponent),
    static_cast<float>(srgb.greenComponent), static_cast<float>(srgb.blueComponent),
    static_cast<float>(srgb.alphaComponent));
}

NSColor* nativeFromColor(const graphics::Color color) {
  return [NSColor colorWithSRGBRed:color.red green:color.green
    blue:color.blue alpha:color.alpha];
}

@interface CoralNativeColorWellTarget : NSObject {
@public
  std::function<void(graphics::Color)>* handler;
}
@property(nonatomic, weak) NSColorWell* well;
- (void)changed:(id)sender;
@end

@implementation CoralNativeColorWellTarget

- (void)dealloc {
  delete handler;
  handler = nullptr;
}

- (void)changed:(id)sender {
  (void)sender;
  const graphics::Color color = colorFromNative(_well != nil ? _well.color : nil);
  if (handler != nullptr && *handler) (*handler)(color);
}

@end

namespace coral::ui::macos {

void* createNativeColorWellView(const graphics::Color initial,
    std::function<void(graphics::Color)> onChange) {
  if (![NSThread isMainThread]) return nullptr;
  NSColorWell* well = [[NSColorWell alloc] initWithFrame:NSMakeRect(0, 0, 44, 28)];
  well.color = nativeFromColor(initial);
  if (onChange) {
    CoralNativeColorWellTarget* target = [[CoralNativeColorWellTarget alloc] init];
    target->handler = new std::function<void(graphics::Color)>(std::move(onChange));
    target.well = well;
    well.target = target;
    well.action = @selector(changed:);
    objc_setAssociatedObject(well, &kNativeColorWellTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)well;
}

void setNativeColorWellColor(void* nativeView, const graphics::Color color) {
  if (nativeView == nullptr) return;
  NSColorWell* well = (__bridge NSColorWell*)nativeView;
  if ([well isKindOfClass:[NSColorWell class]]) well.color = nativeFromColor(color);
}

graphics::Color nativeColorWellColor(void* nativeView) {
  if (nativeView == nullptr) return {};
  NSColorWell* well = (__bridge NSColorWell*)nativeView;
  if (![well isKindOfClass:[NSColorWell class]]) return {};
  return colorFromNative(well.color);
}

} // namespace coral::ui::macos
