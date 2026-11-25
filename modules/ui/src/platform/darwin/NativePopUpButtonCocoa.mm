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

#include <ui/macos/NativePopUpButton.h>

#include "NativePopUpButtonInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <utility>

char kNativePopUpButtonTargetKey;

@interface CoralNativePopUpButtonTarget : NSObject {
@public
  std::function<void(int, std::string_view)>* handler;
}
@property(nonatomic, weak) NSPopUpButton* button;
- (void)changed:(id)sender;
@end

@implementation CoralNativePopUpButtonTarget

- (void)dealloc {
  delete handler;
  handler = nullptr;
}

- (void)changed:(id)sender {
  (void)sender;
  const NSInteger index = _button != nil ? _button.indexOfSelectedItem : -1;
  NSString* title = index >= 0 && _button != nil ? [_button itemTitleAtIndex:index] : @"";
  if (handler != nullptr && *handler) {
    (*handler)(static_cast<int>(index), title == nil ? std::string_view{} :
      std::string_view(title.UTF8String));
  }
}

@end

namespace coral::ui::macos {

void* createNativePopUpButtonView(std::vector<std::string> items, const int selected,
    std::function<void(int, std::string_view)> onSelect) {
  if (![NSThread isMainThread]) return nullptr;
  NSPopUpButton* button = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(0, 0, 180, 28)
    pullsDown:NO];
  NSMutableArray<NSString*>* titles = [NSMutableArray arrayWithCapacity:items.size()];
  for (const std::string& item: items) {
    [titles addObject:[NSString stringWithUTF8String:item.c_str()]];
  }
  [button addItemsWithTitles:titles];
  if (!items.empty()) {
    [button selectItemAtIndex:std::clamp(selected, 0, static_cast<int>(items.size()) - 1)];
  }
  if (onSelect) {
    CoralNativePopUpButtonTarget* target = [[CoralNativePopUpButtonTarget alloc] init];
    target->handler = new std::function<void(int, std::string_view)>(std::move(onSelect));
    target.button = button;
    button.target = target;
    button.action = @selector(changed:);
    objc_setAssociatedObject(button, &kNativePopUpButtonTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)button;
}

void setNativePopUpButtonItems(void* nativeView, std::vector<std::string> items,
    const int selected) {
  if (nativeView == nullptr) return;
  NSPopUpButton* button = (__bridge NSPopUpButton*)nativeView;
  if (![button isKindOfClass:[NSPopUpButton class]]) return;
  NSMutableArray<NSString*>* titles = [NSMutableArray arrayWithCapacity:items.size()];
  for (const std::string& item: items) {
    [titles addObject:[NSString stringWithUTF8String:item.c_str()]];
  }
  [button removeAllItems];
  [button addItemsWithTitles:titles];
  if (!items.empty()) {
    [button selectItemAtIndex:std::clamp(selected, 0, static_cast<int>(items.size()) - 1)];
  }
}

int nativePopUpButtonCount(void* nativeView) {
  NSPopUpButton* button = nativeView != nullptr ? (__bridge NSPopUpButton*)nativeView : nil;
  return [button isKindOfClass:[NSPopUpButton class]] ? static_cast<int>(button.numberOfItems) : 0;
}

int nativePopUpButtonSelectedIndex(void* nativeView) {
  NSPopUpButton* button = nativeView != nullptr ? (__bridge NSPopUpButton*)nativeView : nil;
  return [button isKindOfClass:[NSPopUpButton class]]
    ? static_cast<int>(button.indexOfSelectedItem) : -1;
}

void setNativePopUpButtonSelectedIndex(void* nativeView, const int index) {
  NSPopUpButton* button = nativeView != nullptr ? (__bridge NSPopUpButton*)nativeView : nil;
  if ([button isKindOfClass:[NSPopUpButton class]] && index >= 0 && index < button.numberOfItems) {
    [button selectItemAtIndex:index];
  }
}

std::string nativePopUpButtonItemTitle(void* nativeView, const int index) {
  NSPopUpButton* button = nativeView != nullptr ? (__bridge NSPopUpButton*)nativeView : nil;
  if (![button isKindOfClass:[NSPopUpButton class]] || index < 0 || index >= button.numberOfItems) {
    return {};
  }
  NSString* title = [button itemTitleAtIndex:index];
  return title != nil ? std::string(title.UTF8String) : std::string();
}

} // namespace coral::ui::macos
