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

#include <ui/macos/NativeDatePicker.h>

#include "NativeDatePickerInternal.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <utility>

char kNativeDatePickerTargetKey;

NSDate* dateFromDay(const std::chrono::year_month_day& day) {
  NSDateComponents* components = [[NSDateComponents alloc] init];
  components.year = static_cast<NSInteger>(static_cast<int>(day.year()));
  components.month = static_cast<NSInteger>(static_cast<unsigned>(day.month()));
  components.day = static_cast<NSInteger>(static_cast<unsigned>(day.day()));
  return [[NSCalendar currentCalendar] dateFromComponents:components];
}

std::chrono::year_month_day dayFromDate(NSDate* date) {
  if (date == nil) return {};
  NSDateComponents* components = [[NSCalendar currentCalendar]
    components:(NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay) fromDate:date];
  return std::chrono::year_month_day{ std::chrono::year{static_cast<int>(components.year)},
    std::chrono::month{static_cast<unsigned>(components.month)},
    std::chrono::day{static_cast<unsigned>(components.day)} };
}

@interface CoralNativeDatePickerTarget : NSObject {
@public
  std::function<void(std::chrono::year_month_day)>* handler;
}
@property(nonatomic, weak) NSDatePicker* picker;
- (void)changed:(id)sender;
@end

@implementation CoralNativeDatePickerTarget

- (void)dealloc {
  delete handler;
  handler = nullptr;
}

- (void)changed:(id)sender {
  (void)sender;
  const auto day = dayFromDate(_picker != nil ? _picker.dateValue : nil);
  if (handler != nullptr && *handler) (*handler)(day);
}

@end

namespace coral::ui::macos {

void* createNativeDatePickerView(const std::chrono::year_month_day initial,
    std::function<void(std::chrono::year_month_day)> onChange) {
  if (![NSThread isMainThread]) return nullptr;
  NSDatePicker* picker = [[NSDatePicker alloc] initWithFrame:NSMakeRect(0, 0, 220, 28)];
  picker.datePickerStyle = NSDatePickerStyleTextFieldAndStepper;
  picker.datePickerMode = NSDatePickerModeSingle;
  picker.datePickerElements = NSDatePickerElementFlagYearMonthDay;
  picker.dateValue = dateFromDay(initial);
  if (onChange) {
    CoralNativeDatePickerTarget* target = [[CoralNativeDatePickerTarget alloc] init];
    target->handler = new std::function<void(std::chrono::year_month_day)>(
      std::move(onChange));
    target.picker = picker;
    picker.target = target;
    picker.action = @selector(changed:);
    objc_setAssociatedObject(picker, &kNativeDatePickerTargetKey, target,
      OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return (__bridge_retained void*)picker;
}

void setNativeDatePickerDate(void* nativeView, const std::chrono::year_month_day date) {
  NSDatePicker* picker = nativeView != nullptr ? (__bridge NSDatePicker*)nativeView : nil;
  if ([picker isKindOfClass:[NSDatePicker class]]) picker.dateValue = dateFromDay(date);
}

std::chrono::year_month_day nativeDatePickerDate(void* nativeView) {
  NSDatePicker* picker = nativeView != nullptr ? (__bridge NSDatePicker*)nativeView : nil;
  return [picker isKindOfClass:[NSDatePicker class]] ? dayFromDate(picker.dateValue) : std::chrono::year_month_day{};
}

} // namespace coral::ui::macos
