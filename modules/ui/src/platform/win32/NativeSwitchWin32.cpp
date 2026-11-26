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

#include <ui/win32/NativeSwitch.h>

#include "NativeControlWin32.h"

#include <utility>

namespace coral::ui::win32 {

NativeSwitch::NativeSwitch(void* nativeView, const int controlId, std::string title,
    const bool on, ChangeHandler onChange)
    : NativeViewLayer(nativeView),
      mHost(std::make_shared<detail::NativeControlHost>(static_cast<HWND>(nativeView), controlId)),
      mTitle(std::move(title)), mOnChange(std::move(onChange)) {
  mHost->setHandlers([this](const int notificationCode) {
    if (notificationCode == BN_CLICKED) {
      if (mOnChange) mOnChange(isOn());
      return true;
    }
    return false;
  });
  setTitle(mTitle);
  setOn(on);
}

NativeSwitch::~NativeSwitch() = default;

std::shared_ptr<NativeSwitch> NativeSwitch::create(std::string title, const bool on,
    ChangeHandler onChange) {
  const int controlId = detail::nextControlId();
  HWND control = detail::createControl(L"BUTTON",
    WS_CHILD | WS_CLIPSIBLINGS | BS_AUTOCHECKBOX | BS_LEFT, 0, controlId);
  if (control == nullptr) return {};
  auto native = std::shared_ptr<NativeSwitch>(
    new NativeSwitch(control, controlId, std::move(title), on, std::move(onChange)));
  native->setPreferredSize(Size{ 100.0f, 28.0f });
  return native;
}

void NativeSwitch::setOn(const bool on) {
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SendMessageW(control, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
  }
}

bool NativeSwitch::isOn() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr &&
    ::SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void NativeSwitch::setTitle(std::string title) {
  mTitle = std::move(title);
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SetWindowTextW(control, detail::toWide(mTitle).c_str());
  }
}

void NativeSwitch::setEnabled(const bool enabled) {
  mEnabled = enabled;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::EnableWindow(control, enabled ? TRUE : FALSE);
  }
}

bool NativeSwitch::enabled() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? ::IsWindowEnabled(control) != FALSE : mEnabled;
}

void NativeSwitch::setControlSize(const NativeControlSize size) {
  mControlSize = size;
}

void NativeSwitch::onWindowChanged(Window* previous, Window* current) {
  NativeViewLayer::onWindowChanged(previous, current);
  if (current != nullptr) mHost->attach(current);
  else mHost->detach();
}

} // namespace coral::ui::win32
