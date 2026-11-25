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

#include "NativeProgressIndicatorInternal.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>

namespace coral::ui::macos {

void* createNativeProgressIndicatorView(const bool indeterminate, const bool spinning) {
  if (![NSThread isMainThread]) return nullptr;
  NSProgressIndicator* indicator = [[NSProgressIndicator alloc]
    initWithFrame:NSMakeRect(0, 0, 160, 12)];
  indicator.style = spinning ? NSProgressIndicatorStyleSpinning : NSProgressIndicatorStyleBar;
  indicator.minValue = 0.0;
  indicator.maxValue = 1.0;
  indicator.doubleValue = 0.0;
  indicator.indeterminate = indeterminate ? YES : NO;
  indicator.displayedWhenStopped = YES;
  if (indeterminate) [indicator startAnimation:nil];
  return (__bridge_retained void*)indicator;
}

void setNativeProgressIndicatorIndeterminate(void* nativeView, const bool indeterminate) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  if (![indicator isKindOfClass:[NSProgressIndicator class]]) return;
  indicator.indeterminate = indeterminate ? YES : NO;
  if (indeterminate) {
    [indicator startAnimation:nil];
  } else {
    [indicator stopAnimation:nil];
  }
}

bool nativeProgressIndicatorIndeterminate(void* nativeView) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  return [indicator isKindOfClass:[NSProgressIndicator class]] && indicator.isIndeterminate;
}

void setNativeProgressIndicatorValue(void* nativeView, const double value) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  if ([indicator isKindOfClass:[NSProgressIndicator class]]) {
    indicator.doubleValue = std::clamp(value, indicator.minValue, indicator.maxValue);
  }
}

void setNativeProgressIndicatorStyle(void* nativeView, const bool spinning) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  if (![indicator isKindOfClass:[NSProgressIndicator class]]) return;
  indicator.style = spinning ? NSProgressIndicatorStyleSpinning : NSProgressIndicatorStyleBar;
}

bool nativeProgressIndicatorStyle(void* nativeView) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  return [indicator isKindOfClass:[NSProgressIndicator class]] &&
    indicator.style == NSProgressIndicatorStyleSpinning;
}

void setNativeProgressIndicatorSize(void* nativeView, const int size) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  if (![indicator isKindOfClass:[NSProgressIndicator class]]) return;
  indicator.controlSize = static_cast<NSControlSize>(std::clamp(size, 0, 2));
}

int nativeProgressIndicatorSize(void* nativeView) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  return [indicator isKindOfClass:[NSProgressIndicator class]]
    ? static_cast<int>(indicator.controlSize) : 1;
}

void setNativeProgressIndicatorDisplayedWhenStopped(void* nativeView, const bool displayed) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  if ([indicator isKindOfClass:[NSProgressIndicator class]]) {
    indicator.displayedWhenStopped = displayed ? YES : NO;
  }
}

bool nativeProgressIndicatorDisplayedWhenStopped(void* nativeView) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  return [indicator isKindOfClass:[NSProgressIndicator class]] && indicator.displayedWhenStopped;
}

double nativeProgressIndicatorValue(void* nativeView) {
  NSProgressIndicator* indicator = nativeView != nullptr
    ? (__bridge NSProgressIndicator*)nativeView : nil;
  return [indicator isKindOfClass:[NSProgressIndicator class]] ? indicator.doubleValue : 0.0;
}

} // namespace coral::ui::macos
