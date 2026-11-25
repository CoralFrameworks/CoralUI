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

#include <ui/macos/NativeSlider.h>

#include "NativeSliderInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <utility>

char kNativeSliderTargetKey;

@interface CoralNativeSliderTarget : NSObject {
@public
  std::function<void(double)>* handler;
}
@property(nonatomic, weak) NSSlider* slider;
- (void)changed:(id)sender;
@end

@implementation CoralNativeSliderTarget

- (void)dealloc {
  delete handler;
  handler = nullptr;
}

- (void)changed:(id)sender {
  (void)sender;
  const double value = _slider != nil ? _slider.doubleValue : 0.0;
  if (handler != nullptr && *handler) (*handler)(value);
}

@end

namespace coral::ui::macos {

void* createNativeSliderView(const double minimum, const double maximum, const double value,
    std::function<void(double)> onChange) {
  if (![NSThread isMainThread]) return nullptr;
  NSSlider* slider = [[NSSlider alloc] initWithFrame:NSMakeRect(0, 0, 220, 28)];
  slider.minValue = minimum;
  slider.maxValue = std::max(minimum + 1e-9, maximum);
  slider.doubleValue = std::clamp(value, minimum, maximum);
  slider.continuous = YES;
  if (onChange) {
    CoralNativeSliderTarget* target = [[CoralNativeSliderTarget alloc] init];
    target->handler = new std::function<void(double)>(std::move(onChange));
    target.slider = slider;
    slider.target = target;
    slider.action = @selector(changed:);
    objc_setAssociatedObject(slider, &kNativeSliderTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)slider;
}

void setNativeSliderRange(void* nativeView, const double minimum, const double maximum) {
  NSSlider* slider = nativeView != nullptr ? (__bridge NSSlider*)nativeView : nil;
  if (![slider isKindOfClass:[NSSlider class]]) return;
  slider.minValue = minimum;
  slider.maxValue = std::max(minimum + 1e-9, maximum);
}

void setNativeSliderValue(void* nativeView, const double value) {
  NSSlider* slider = nativeView != nullptr ? (__bridge NSSlider*)nativeView : nil;
  if ([slider isKindOfClass:[NSSlider class]]) {
    slider.doubleValue = std::clamp(value, slider.minValue, slider.maxValue);
  }
}

double nativeSliderValue(void* nativeView) {
  NSSlider* slider = nativeView != nullptr ? (__bridge NSSlider*)nativeView : nil;
  return [slider isKindOfClass:[NSSlider class]] ? slider.doubleValue : 0.0;
}

void setNativeSliderContinuous(void* nativeView, const bool continuous) {
  NSSlider* slider = nativeView != nullptr ? (__bridge NSSlider*)nativeView : nil;
  if ([slider isKindOfClass:[NSSlider class]]) slider.continuous = continuous ? YES : NO;
}

} // namespace coral::ui::macos
