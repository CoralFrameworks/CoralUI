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

#include <ui/macos/NativeSwitch.h>

#include "NativeSwitchInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <string>
#include <utility>

char kNativeSwitchTargetKey;

@interface CoralNativeSwitchTarget : NSObject {
@public
  std::function<void(bool)>* handler;
}
@property(nonatomic, weak) NSButton* button;
- (void)changed:(id)sender;
@end

@implementation CoralNativeSwitchTarget

- (void)dealloc {
  delete handler;
  handler = nullptr;
}

- (void)changed:(id)sender {
  (void)sender;
  const bool on = _button != nil && _button.state == NSControlStateValueOn;
  if (handler != nullptr && *handler) (*handler)(on);
}

@end

namespace coral::ui::macos {

void* createNativeSwitchView(const std::string_view title, const bool on,
    std::function<void(bool)> onChange) {
  if (![NSThread isMainThread]) return nullptr;
  NSButton* button = [NSButton buttonWithTitle:
    [NSString stringWithUTF8String:std::string(title).c_str()] target:nil action:nil];
  [button setButtonType:NSButtonTypeSwitch];
  button.state = on ? NSControlStateValueOn : NSControlStateValueOff;
  if (onChange) {
    CoralNativeSwitchTarget* target = [[CoralNativeSwitchTarget alloc] init];
    target->handler = new std::function<void(bool)>(std::move(onChange));
    target.button = button;
    button.target = target;
    button.action = @selector(changed:);
    objc_setAssociatedObject(button, &kNativeSwitchTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)button;
}

void setNativeSwitchOn(void* nativeView, const bool on) {
  NSButton* button = nativeView != nullptr ? (__bridge NSButton*)nativeView : nil;
  if ([button isKindOfClass:[NSButton class]]) {
    button.state = on ? NSControlStateValueOn : NSControlStateValueOff;
  }
}

bool nativeSwitchIsOn(void* nativeView) {
  NSButton* button = nativeView != nullptr ? (__bridge NSButton*)nativeView : nil;
  return [button isKindOfClass:[NSButton class]] && button.state == NSControlStateValueOn;
}

void setNativeSwitchTitle(void* nativeView, const std::string_view title) {
  NSButton* button = nativeView != nullptr ? (__bridge NSButton*)nativeView : nil;
  if ([button isKindOfClass:[NSButton class]]) {
    button.title = [NSString stringWithUTF8String:std::string(title).c_str()];
  }
}

} // namespace coral::ui::macos
