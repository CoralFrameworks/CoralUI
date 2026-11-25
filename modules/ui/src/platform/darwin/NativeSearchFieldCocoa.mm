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

#include <ui/macos/NativeSearchField.h>

#include "NativeSearchFieldInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <string>
#include <utility>

char kNativeSearchFieldTargetKey;

@interface CoralNativeSearchFieldTarget : NSObject {
@public
  std::function<void(std::string_view)>* handler;
}
@property(nonatomic, weak) NSSearchField* field;
- (void)changed:(id)sender;
@end

@implementation CoralNativeSearchFieldTarget

- (void)dealloc {
  delete handler;
  handler = nullptr;
}

- (void)changed:(id)sender {
  (void)sender;
  NSString* value = _field != nil ? _field.stringValue : @"";
  if (handler != nullptr && *handler) {
    (*handler)(value == nil ? std::string_view{} :
      std::string_view(value.UTF8String));
  }
}

@end

namespace coral::ui::macos {

void* createNativeSearchFieldView(const std::string_view placeholder,
    std::function<void(std::string_view)> onChange) {
  if (![NSThread isMainThread]) return nullptr;
  NSSearchField* field = [[NSSearchField alloc] initWithFrame:NSMakeRect(0, 0, 220, 28)];
  field.placeholderString = placeholder.empty() ? @""
    : [NSString stringWithUTF8String:std::string(placeholder).c_str()];
  if (onChange) {
    CoralNativeSearchFieldTarget* target = [[CoralNativeSearchFieldTarget alloc] init];
    target->handler = new std::function<void(std::string_view)>(std::move(onChange));
    target.field = field;
    field.target = target;
    field.action = @selector(changed:);
    objc_setAssociatedObject(field, &kNativeSearchFieldTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)field;
}

void setNativeSearchFieldText(void* nativeView, const std::string_view text) {
  if (nativeView == nullptr) return;
  NSSearchField* field = (__bridge NSSearchField*)nativeView;
  if ([field isKindOfClass:[NSSearchField class]]) {
    field.stringValue = [NSString stringWithUTF8String:std::string(text).c_str()];
  }
}

std::string nativeSearchFieldText(void* nativeView) {
  if (nativeView == nullptr) return {};
  NSSearchField* field = (__bridge NSSearchField*)nativeView;
  if (![field isKindOfClass:[NSSearchField class]] || field.stringValue == nil) return {};
  return std::string(field.stringValue.UTF8String);
}

void setNativeSearchFieldPlaceholder(void* nativeView, const std::string_view placeholder) {
  if (nativeView == nullptr) return;
  NSSearchField* field = (__bridge NSSearchField*)nativeView;
  if ([field isKindOfClass:[NSSearchField class]]) {
    field.placeholderString = [NSString stringWithUTF8String:std::string(placeholder).c_str()];
  }
}

} // namespace coral::ui::macos
