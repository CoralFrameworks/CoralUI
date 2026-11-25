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

#include <ui/Notification.h>

#import <Cocoa/Cocoa.h>
#import <UserNotifications/UserNotifications.h>

#include <string>

namespace coral::ui {

namespace {

NotificationAuthorization authorizationFromStatus(const UNAuthorizationStatus status) {
  switch (status) {
    case UNAuthorizationStatusNotDetermined: return NotificationAuthorization::NotDetermined;
    case UNAuthorizationStatusDenied: return NotificationAuthorization::Denied;
    case UNAuthorizationStatusAuthorized: return NotificationAuthorization::Authorized;
    case UNAuthorizationStatusProvisional: return NotificationAuthorization::Provisional;
    default: return NotificationAuthorization::Denied;
  }
}

bool hasBundleIdentifier() {
  return [NSBundle mainBundle].bundleIdentifier.length > 0;
}

} // namespace

bool postUserNotification(const std::string_view title, const std::string_view body) {
  if (![NSThread isMainThread]) return false;
  @autoreleasepool {
    NSString* nativeTitle = title.empty() ? nil
      : [NSString stringWithUTF8String:std::string(title).c_str()];
    NSString* nativeBody = body.empty() ? nil
      : [NSString stringWithUTF8String:std::string(body).c_str()];
    if (nativeTitle.length == 0 && nativeBody.length == 0) return false;
    if (@available(macOS 10.14, *)) {
      if (hasBundleIdentifier()) {
        UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
        content.title = nativeTitle ?: @"";
        content.body = nativeBody ?: @"";
        UNNotificationRequest* request = [UNNotificationRequest
          requestWithIdentifier:[[NSUUID UUID] UUIDString] content:content trigger:nil];
        [[UNUserNotificationCenter currentNotificationCenter]
          addNotificationRequest:request withCompletionHandler:nil];
        return true;
      }
    }
    NSUserNotification* notification = [[NSUserNotification alloc] init];
    notification.title = nativeTitle;
    notification.informativeText = nativeBody;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
#pragma clang diagnostic pop
    return true;
  }
}

bool requestNotificationAuthorization(NotificationAuthorizationHandler handler) {
  if (![NSThread isMainThread] || !hasBundleIdentifier()) {
    if (handler) handler(NotificationAuthorization::Unavailable);
    return false;
  }
  if (@available(macOS 10.14, *)) {
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    [center requestAuthorizationWithOptions:
      (UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge)
      completionHandler:^(BOOL granted, NSError* error) {
        (void)error;
        const NotificationAuthorization result = granted ? NotificationAuthorization::Authorized
                                                         : NotificationAuthorization::Denied;
        if (handler) dispatch_async(dispatch_get_main_queue(), ^{ handler(result); });
      }];
    return true;
  }
  if (handler) handler(NotificationAuthorization::Unavailable);
  return false;
}

void queryNotificationAuthorization(NotificationAuthorizationHandler handler) {
  if (!hasBundleIdentifier()) {
    if (handler) handler(NotificationAuthorization::Unavailable);
    return;
  }
  if (@available(macOS 10.14, *)) {
    [[UNUserNotificationCenter currentNotificationCenter]
      getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* settings) {
        const NotificationAuthorization result = authorizationFromStatus(settings.authorizationStatus);
        if (handler) dispatch_async(dispatch_get_main_queue(), ^{ handler(result); });
      }];
    return;
  }
  if (handler) handler(NotificationAuthorization::Unavailable);
}

bool supportsUserNotifications() noexcept {
  return true;
}

} // namespace coral::ui
