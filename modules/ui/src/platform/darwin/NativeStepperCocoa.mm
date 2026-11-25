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

#include "NativeStepperInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <algorithm>
#include <utility>

namespace coral::ui::macos {
namespace {
char kNativeStepperTargetKey;
}

} // namespace coral::ui::macos

@interface CoralNativeStepperTarget : NSObject {
@public
  std::function<void(double)>* handler;
}
@property(nonatomic, weak) NSStepper* stepper;
- (void)changed:(id)sender;
@end

@implementation CoralNativeStepperTarget

- (void)dealloc {
  delete handler;
  handler = nullptr;
}

- (void)changed:(id)sender {
  (void)sender;
  const double value = _stepper != nil ? _stepper.doubleValue : 0.0;
  if (handler != nullptr && *handler) (*handler)(value);
}

@end

namespace coral::ui::macos {

void* createNativeStepperView(const double minimum, const double maximum, const double value,
    std::function<void(double)> onChange) {
  if (![NSThread isMainThread]) return nullptr;
  NSStepper* stepper = [[NSStepper alloc] initWithFrame:NSMakeRect(0, 0, 19, 27)];
  stepper.minValue = minimum;
  stepper.maxValue = std::max(minimum + 1e-9, maximum);
  stepper.doubleValue = std::clamp(value, minimum, maximum);
  stepper.increment = 1.0;
  stepper.valueWraps = NO;
  stepper.autorepeat = YES;
  if (onChange) {
    CoralNativeStepperTarget* target = [[CoralNativeStepperTarget alloc] init];
    target->handler = new std::function<void(double)>(std::move(onChange));
    target.stepper = stepper;
    stepper.target = target;
    stepper.action = @selector(changed:);
    objc_setAssociatedObject(stepper, &kNativeStepperTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)stepper;
}

void setNativeStepperRange(void* nativeView, const double minimum, const double maximum) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  if (![stepper isKindOfClass:[NSStepper class]]) return;
  stepper.minValue = minimum;
  stepper.maxValue = std::max(minimum + 1e-9, maximum);
  stepper.doubleValue = std::clamp(stepper.doubleValue, stepper.minValue, stepper.maxValue);
}

void setNativeStepperValue(void* nativeView, const double value) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  if ([stepper isKindOfClass:[NSStepper class]]) {
    stepper.doubleValue = std::clamp(value, stepper.minValue, stepper.maxValue);
  }
}

double nativeStepperValue(void* nativeView) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  return [stepper isKindOfClass:[NSStepper class]] ? stepper.doubleValue : 0.0;
}

double nativeStepperIncrement(void* nativeView) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  return [stepper isKindOfClass:[NSStepper class]] ? stepper.increment : 0.0;
}

void setNativeStepperSize(void* nativeView, const int size) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  if ([stepper isKindOfClass:[NSStepper class]]) {
    stepper.controlSize = static_cast<NSControlSize>(std::clamp(size, 0, 2));
  }
}

int nativeStepperSize(void* nativeView) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  return [stepper isKindOfClass:[NSStepper class]] ? static_cast<int>(stepper.controlSize) : 1;
}

void setNativeStepperValueWraps(void* nativeView, const bool wraps) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  if ([stepper isKindOfClass:[NSStepper class]]) stepper.valueWraps = wraps ? YES : NO;
}

void setNativeStepperAutorepeat(void* nativeView, const bool autorepeat) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  if ([stepper isKindOfClass:[NSStepper class]]) stepper.autorepeat = autorepeat ? YES : NO;
}

void setNativeStepperIncrement(void* nativeView, const double increment) {
  NSStepper* stepper = nativeView != nullptr ? (__bridge NSStepper*)nativeView : nil;
  if ([stepper isKindOfClass:[NSStepper class]]) {
    stepper.increment = std::max(1e-9, increment);
  }
}

} // namespace coral::ui::macos
