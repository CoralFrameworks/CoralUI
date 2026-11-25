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

#include <ui/DatePickerPanel.h>

#import <Cocoa/Cocoa.h>

#include <mutex>
#include <utility>

namespace {

std::mutex gHandlerMutex;
coral::ui::DatePickerHandler gHandler;
NSPanel* gPanel = nil;
NSDatePicker* gPicker = nil;
id gTarget = nil;

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

} // namespace

@interface CoralDatePickerTarget : NSObject
- (void)dateChanged:(id)sender;
- (void)done:(id)sender;
- (void)cancel:(id)sender;
@end

@implementation CoralDatePickerTarget

- (void)dateChanged:(id)sender {
  (void)sender;
  const auto day = dayFromDate(gPicker.dateValue);
  coral::ui::DatePickerHandler handler;
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    handler = gHandler;
  }
  if (handler) handler(day);
}

- (void)done:(id)sender {
  (void)sender;
  const auto day = dayFromDate(gPicker.dateValue);
  coral::ui::DatePickerHandler handler;
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    handler = gHandler;
  }
  if (gPanel != nil) [gPanel orderOut:nil];
  if (handler) handler(day);
}

- (void)cancel:(id)sender {
  (void)sender;
  if (gPanel != nil) [gPanel orderOut:nil];
}

@end

namespace coral::ui {

bool showNativeDatePicker(const std::chrono::year_month_day initial,
    DatePickerHandler handler) {
  if (![NSThread isMainThread]) return false;
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    gHandler = std::move(handler);
  }
  if (gPanel == nil) {
    gPanel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 300, 150)
      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
        backing:NSBackingStoreBuffered defer:NO];
    gPanel.title = @"Date";
    gPicker = [[NSDatePicker alloc] initWithFrame:NSMakeRect(20, 85, 260, 32)];
    gPicker.datePickerStyle = NSDatePickerStyleTextFieldAndStepper;
    gPicker.datePickerMode = NSDatePickerModeSingle;
    gPicker.datePickerElements = NSDatePickerElementFlagYearMonthDay;
    gTarget = [[CoralDatePickerTarget alloc] init];
    gPicker.target = gTarget;
    gPicker.action = @selector(dateChanged:);
    [gPanel.contentView addSubview:gPicker];
    // The panel needs an explicit commit action; without one, changing the picker updated the
    // handler but left the user with no way to finish the selection.
    NSButton* cancelButton = [NSButton buttonWithTitle:@"Cancel"
      target:gTarget action:@selector(cancel:)];
    cancelButton.frame = NSMakeRect(140, 20, 70, 28);
    cancelButton.bezelStyle = NSBezelStyleRounded;
    [gPanel.contentView addSubview:cancelButton];
    NSButton* doneButton = [NSButton buttonWithTitle:@"Done"
      target:gTarget action:@selector(done:)];
    doneButton.frame = NSMakeRect(215, 20, 70, 28);
    doneButton.bezelStyle = NSBezelStyleRounded;
    doneButton.keyEquivalent = @"\r";
    [gPanel.contentView addSubview:doneButton];
  }
  gPicker.dateValue = dateFromDay(initial);
  [gPanel center];
  [gPanel makeKeyAndOrderFront:nil];
  return true;
}

void hideNativeDatePicker() {
  if (gPanel != nil) [gPanel orderOut:nil];
  gPanel = nil;
  gPicker = nil;
  gTarget = nil;
  const std::lock_guard<std::mutex> lock(gHandlerMutex);
  gHandler = {};
}

bool isNativeDatePickerVisible() noexcept {
  return gPanel != nil && gPanel.isVisible;
}

bool supportsNativeDatePicker() noexcept {
  return true;
}

} // namespace coral::ui
