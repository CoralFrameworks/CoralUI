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

#include <ui/macos/NativeButton.h>

#include "NativeButtonInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <cstddef>
#include <utility>

char kNativeButtonTargetKey;

@interface CoralNativeButtonTarget : NSObject
@property(nonatomic, copy) void (^handler)(void);
- (void)click:(id)sender;
@end

@implementation CoralNativeButtonTarget

- (void)click:(id)sender {
  (void)sender;
  if (self.handler != nil) self.handler();
}

@end

namespace coral::ui::macos {

void* createNativeButtonView(const std::string_view title, std::function<void()> onClick) {
  if (![NSThread isMainThread]) return nullptr;
  NSButton* button = [NSButton buttonWithTitle:
    [NSString stringWithUTF8String:std::string(title).c_str()] target:nil action:nil];
  button.bezelStyle = NSBezelStyleRounded;
  button.translatesAutoresizingMaskIntoConstraints = YES;
  if (onClick) {
    CoralNativeButtonTarget* target = [[CoralNativeButtonTarget alloc] init];
    target.handler = ^{ onClick(); };
    button.target = target;
    button.action = @selector(click:);
    objc_setAssociatedObject(button, &kNativeButtonTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)button;
}

void setNativeButtonSize(void* nativeView, const int size) {
  NSButton* button = nativeView != nullptr ? (__bridge NSButton*)nativeView : nil;
  if ([button isKindOfClass:[NSButton class]]) {
    button.controlSize = static_cast<NSControlSize>(std::clamp(size, 0, 2));
  }
}

int nativeButtonSize(void* nativeView) {
  NSButton* button = nativeView != nullptr ? (__bridge NSButton*)nativeView : nil;
  return [button isKindOfClass:[NSButton class]] ? static_cast<int>(button.controlSize) : 1;
}

void setNativeButtonTitle(void* nativeView, const std::string_view title) {
  if (nativeView == nullptr) return;
  NSButton* button = (__bridge NSButton*)nativeView;
  if ([button isKindOfClass:[NSButton class]]) {
    button.title = [NSString stringWithUTF8String:std::string(title).c_str()];
  }
}

} // namespace coral::ui::macos
