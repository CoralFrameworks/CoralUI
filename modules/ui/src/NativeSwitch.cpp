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

#include <ui/macos/NativeSwitch.h>

#include "NativeSwitchInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

#include <utility>

namespace coral::ui::macos {

NativeSwitch::NativeSwitch(void* nativeView, std::string title)
    : NativeViewLayer(nativeView), mTitle(std::move(title)) {}

NativeSwitch::~NativeSwitch() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeSwitch> NativeSwitch::create(std::string title, const bool on,
    ChangeHandler onChange) {
  void* view = createNativeSwitchView(title, on, std::move(onChange));
  if (view == nullptr) return {};
  auto control = std::shared_ptr<NativeSwitch>(new NativeSwitch(view, std::move(title)));
  applyNativeViewPreferredSize(*control, view);
  return control;
}

void NativeSwitch::setOn(const bool on) {
  if (nativeView() != nullptr) setNativeSwitchOn(nativeView(), on);
}

bool NativeSwitch::isOn() const {
  return nativeView() != nullptr && nativeSwitchIsOn(nativeView());
}

void NativeSwitch::setTitle(std::string title) {
  mTitle = std::move(title);
  if (nativeView() != nullptr) setNativeSwitchTitle(nativeView(), mTitle);
}

void NativeSwitch::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeViewControlSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeSwitch::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeViewControlSize(nativeView()));
}

void NativeSwitch::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeSwitch::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
