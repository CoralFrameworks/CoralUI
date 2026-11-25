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

#ifndef CORAL_UI_NATIVEDATEPICKERINTERNAL_H
#define CORAL_UI_NATIVEDATEPICKERINTERNAL_H

#include <chrono>
#include <functional>

namespace coral::ui::macos {

void* createNativeDatePickerView(std::chrono::year_month_day initial,
  std::function<void(std::chrono::year_month_day)> onChange);
void setNativeDatePickerDate(void* nativeView, std::chrono::year_month_day date);
std::chrono::year_month_day nativeDatePickerDate(void* nativeView);

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVEDATEPICKERINTERNAL_H
