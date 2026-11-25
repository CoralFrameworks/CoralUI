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

#ifndef CORAL_UI_NATIVEPOPUPBUTTONINTERNAL_H
#define CORAL_UI_NATIVEPOPUPBUTTONINTERNAL_H

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui::macos {

void* createNativePopUpButtonView(std::vector<std::string> items, int selected,
  std::function<void(int, std::string_view)> onSelect);
void setNativePopUpButtonItems(void* nativeView, std::vector<std::string> items, int selected);
int nativePopUpButtonCount(void* nativeView);
int nativePopUpButtonSelectedIndex(void* nativeView);
void setNativePopUpButtonSelectedIndex(void* nativeView, int index);
std::string nativePopUpButtonItemTitle(void* nativeView, int index);

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVEPOPUPBUTTONINTERNAL_H
