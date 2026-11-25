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

#include <ui/SystemSettings.h>

#import <Cocoa/Cocoa.h>

#include <mutex>
#include <string>
#include <utility>

namespace coral::ui {

namespace {

std::mutex gHandlerMutex;
std::function<void()> gHandler;
bool gObserversInstalled = false;

std::string utf8(NSString* string) {
  if (string == nil) return {};
  const char* value = string.UTF8String;
  return value != nullptr ? std::string(value) : std::string();
}

void localeChanged(NSNotification* notification) {
  (void)notification;
  dispatch_async(dispatch_get_main_queue(), ^{
    std::function<void()> handler;
    {
      const std::lock_guard<std::mutex> lock(gHandlerMutex);
      handler = gHandler;
    }
    if (handler) handler();
  });
}

void installObservers() {
  if (gObserversInstalled) return;
  gObserversInstalled = true;
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserverForName:NSCurrentLocaleDidChangeNotification object:nil
                      queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* note) {
    localeChanged(note);
  }];
  [center addObserverForName:NSSystemTimeZoneDidChangeNotification object:nil
                      queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* note) {
    localeChanged(note);
  }];
}

} // namespace

SystemLocale currentSystemLocale() {
  @autoreleasepool {
    NSLocale* locale = [NSLocale currentLocale];
    NSCalendar* calendar = [NSCalendar currentCalendar];
    NSString* dateFormat = [NSDateFormatter
      dateFormatFromTemplate:@"j" options:0 locale:locale];
    SystemLocale info;
    info.identifier = utf8(locale.localeIdentifier);
    info.language = utf8([locale objectForKey:NSLocaleLanguageCode]);
    info.uses24HourClock = dateFormat != nil &&
      ([dateFormat rangeOfString:@"H"].location != NSNotFound ||
       [dateFormat rangeOfString:@"k"].location != NSNotFound);
    info.firstWeekday = static_cast<int>(calendar.firstWeekday);
    info.decimalSeparator = utf8([locale objectForKey:NSLocaleDecimalSeparator]);
    info.groupingSeparator = utf8([locale objectForKey:NSLocaleGroupingSeparator]);
    if (info.decimalSeparator.empty()) info.decimalSeparator = ".";
    if (info.groupingSeparator.empty()) info.groupingSeparator = ",";
    NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
    formatter.locale = locale;
    NSArray<NSString*>* months = formatter.monthSymbols;
    NSArray<NSString*>* weekdays = formatter.shortWeekdaySymbols;
    for (NSUInteger index = 0; index < info.monthNames.size() && index < months.count; ++index) {
      info.monthNames[index] = utf8(months[index]);
    }
    for (NSUInteger index = 0; index < info.weekdayNames.size() && index < weekdays.count; ++index) {
      info.weekdayNames[index] = utf8(weekdays[index]);
    }
    return info;
  }
}

void setSystemLocaleChangedHandler(std::function<void()> handler) {
  {
    const std::lock_guard<std::mutex> lock(gHandlerMutex);
    gHandler = std::move(handler);
  }
  if (gHandler) installObservers();
}

bool supportsSystemLocaleMonitoring() noexcept {
  return true;
}

} // namespace coral::ui
