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

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

#include <utility>

namespace coral::ui::macos {

NativeDatePicker::NativeDatePicker(void* nativeView) : NativeViewLayer(nativeView) {}

NativeDatePicker::~NativeDatePicker() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeDatePicker> NativeDatePicker::create(
    const std::chrono::year_month_day initial, ChangeHandler onChange) {
  void* view = createNativeDatePickerView(initial, std::move(onChange));
  if (view == nullptr) return {};
  auto picker = std::shared_ptr<NativeDatePicker>(new NativeDatePicker(view));
  applyNativeViewPreferredSize(*picker, view);
  return picker;
}

void NativeDatePicker::setDate(const std::chrono::year_month_day date) {
  if (nativeView() != nullptr) setNativeDatePickerDate(nativeView(), date);
}

std::chrono::year_month_day NativeDatePicker::date() const {
  return nativeView() != nullptr ? nativeDatePickerDate(nativeView())
                                 : std::chrono::year_month_day{};
}

void NativeDatePicker::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeViewControlSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeDatePicker::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeViewControlSize(nativeView()));
}

void NativeDatePicker::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeDatePicker::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
