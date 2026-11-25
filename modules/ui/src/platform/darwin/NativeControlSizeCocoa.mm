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

#include "NativeControlSizeInternal.h"

#include <ui/NativeViewLayer.h>

#import <Cocoa/Cocoa.h>

#include <algorithm>

namespace coral::ui::macos {

void setNativeViewControlSize(void* nativeView, const int size) {
  NSControl* control = nativeView != nullptr ? (__bridge NSControl*)nativeView : nil;
  if ([control isKindOfClass:[NSControl class]]) {
    control.controlSize = static_cast<NSControlSize>(std::clamp(size, 0, 2));
  }
}

int nativeViewControlSize(void* nativeView) {
  NSControl* control = nativeView != nullptr ? (__bridge NSControl*)nativeView : nil;
  return [control isKindOfClass:[NSControl class]] ? static_cast<int>(control.controlSize) : 1;
}

void applyNativeViewPreferredSize(coral::ui::NativeViewLayer& layer, void* nativeView) {
  NSControl* control = nativeView != nullptr ? (__bridge NSControl*)nativeView : nil;
  if (![control isKindOfClass:[NSControl class]]) return;
  NSSize fitting = control.fittingSize;
  if (fitting.width <= 0.0 || fitting.height <= 0.0) fitting = control.frame.size;
  if (fitting.width > 0.0 && fitting.height > 0.0) {
    layer.setPreferredSize({ static_cast<float>(fitting.width),
      static_cast<float>(fitting.height) });
  }
}

void setNativeViewEnabled(void* nativeView, const bool enabled) {
  NSControl* control = nativeView != nullptr ? (__bridge NSControl*)nativeView : nil;
  if ([control isKindOfClass:[NSControl class]]) control.enabled = enabled ? YES : NO;
}

bool nativeViewEnabled(void* nativeView) {
  NSControl* control = nativeView != nullptr ? (__bridge NSControl*)nativeView : nil;
  return [control isKindOfClass:[NSControl class]] && control.enabled;
}

} // namespace coral::ui::macos
