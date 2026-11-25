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

#ifndef CORAL_UI_NOTIFICATION_H
#define CORAL_UI_NOTIFICATION_H

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace coral::ui {

enum class NotificationAuthorization : uint8_t {
  Unavailable,
  NotDetermined,
  Denied,
  Authorized,
  Provisional,
  Ephemeral,
};

using NotificationAuthorizationHandler =
  std::function<void(NotificationAuthorization)>;

/**
 * Delivers a local user notification.
 *
 * macOS uses UserNotifications when the process has a bundle identifier and falls back to
 * NSUserNotificationCenter for command-line tools.
 */
bool postUserNotification(std::string_view title, std::string_view body);

/** Requests alert/sound/badge authorization; the handler runs on the main queue. */
bool requestNotificationAuthorization(NotificationAuthorizationHandler handler = {});

/** Reads the current authorization status; the handler runs on the main queue. */
void queryNotificationAuthorization(NotificationAuthorizationHandler handler);

[[nodiscard]] bool supportsUserNotifications() noexcept;

} // namespace coral::ui

#endif // CORAL_UI_NOTIFICATION_H
