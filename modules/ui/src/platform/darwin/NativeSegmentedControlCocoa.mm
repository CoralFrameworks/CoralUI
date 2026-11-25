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

#include <ui/macos/NativeSegmentedControl.h>

#include "NativeSegmentedControlInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <utility>

char kNativeSegmentedControlTargetKey;

@interface CoralNativeSegmentedControlTarget : NSObject {
@public
  std::function<void(int)>* handler;
}
@property(nonatomic, weak) NSSegmentedControl* control;
- (void)changed:(id)sender;
@end

@implementation CoralNativeSegmentedControlTarget

- (void)dealloc {
  delete handler;
  handler = nullptr;
}

- (void)changed:(id)sender {
  (void)sender;
  const int index = _control != nil ? static_cast<int>(_control.selectedSegment) : -1;
  if (handler != nullptr && *handler) (*handler)(index);
}

@end

namespace coral::ui::macos {

void* createNativeSegmentedControlView(std::vector<std::string> labels, const int selected,
    std::function<void(int)> onSelect) {
  if (![NSThread isMainThread]) return nullptr;
  NSSegmentedControl* control = [[NSSegmentedControl alloc] init];
  control.segmentCount = static_cast<NSInteger>(labels.size());
  control.trackingMode = NSSegmentSwitchTrackingSelectOne;
  control.segmentStyle = NSSegmentStyleRounded;
  for (size_t index = 0; index < labels.size(); ++index) {
    [control setLabel:[NSString stringWithUTF8String:labels[index].c_str()]
           forSegment:static_cast<NSInteger>(index)];
  }
  if (!labels.empty()) {
    control.selectedSegment = std::clamp(selected, 0, static_cast<int>(labels.size()) - 1);
  }
  [control sizeToFit];
  if (onSelect) {
    CoralNativeSegmentedControlTarget* target =
      [[CoralNativeSegmentedControlTarget alloc] init];
    target->handler = new std::function<void(int)>(std::move(onSelect));
    target.control = control;
    control.target = target;
    control.action = @selector(changed:);
    objc_setAssociatedObject(control, &kNativeSegmentedControlTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)control;
}

void setNativeSegmentedControlLabels(void* nativeView, std::vector<std::string> labels) {
  NSSegmentedControl* control = nativeView != nullptr
    ? (__bridge NSSegmentedControl*)nativeView : nil;
  if (![control isKindOfClass:[NSSegmentedControl class]]) return;
  control.segmentCount = static_cast<NSInteger>(labels.size());
  for (size_t index = 0; index < labels.size(); ++index) {
    [control setLabel:[NSString stringWithUTF8String:labels[index].c_str()]
           forSegment:static_cast<NSInteger>(index)];
  }
  [control sizeToFit];
}

int nativeSegmentedControlCount(void* nativeView) {
  NSSegmentedControl* control = nativeView != nullptr
    ? (__bridge NSSegmentedControl*)nativeView : nil;
  return [control isKindOfClass:[NSSegmentedControl class]]
    ? static_cast<int>(control.segmentCount) : 0;
}

int nativeSegmentedControlSelectedIndex(void* nativeView) {
  NSSegmentedControl* control = nativeView != nullptr
    ? (__bridge NSSegmentedControl*)nativeView : nil;
  return [control isKindOfClass:[NSSegmentedControl class]]
    ? static_cast<int>(control.selectedSegment) : -1;
}

void setNativeSegmentedControlSelectedIndex(void* nativeView, const int index) {
  NSSegmentedControl* control = nativeView != nullptr
    ? (__bridge NSSegmentedControl*)nativeView : nil;
  if ([control isKindOfClass:[NSSegmentedControl class]] && index >= 0 &&
      index < control.segmentCount) {
    control.selectedSegment = index;
  }
}

} // namespace coral::ui::macos
