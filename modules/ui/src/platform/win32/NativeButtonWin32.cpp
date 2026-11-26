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

#include <ui/win32/NativeButton.h>

#include "NativeControlWin32.h"

#include <utility>

namespace coral::ui::win32 {

NativeButton::NativeButton(void* nativeView, const int controlId, std::string title,
    ClickHandler onClick)
    : NativeViewLayer(nativeView),
      mHost(std::make_shared<detail::NativeControlHost>(static_cast<HWND>(nativeView), controlId)),
      mTitle(std::move(title)), mOnClick(std::move(onClick)) {
  mHost->setHandlers([this](const int notificationCode) {
    if (notificationCode == BN_CLICKED) {
      if (mOnClick) mOnClick();
      return true;
    }
    return false;
  });
  setTitle(mTitle);
}

NativeButton::~NativeButton() = default;

std::shared_ptr<NativeButton> NativeButton::create(std::string title, ClickHandler onClick) {
  const int controlId = detail::nextControlId();
  HWND control = detail::createControl(L"BUTTON",
    WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON, 0, controlId);
  if (control == nullptr) return {};
  auto button = std::shared_ptr<NativeButton>(
    new NativeButton(control, controlId, std::move(title), std::move(onClick)));
  button->setPreferredSize(Size{ 100.0f, 28.0f });
  return button;
}

void NativeButton::setTitle(std::string title) {
  mTitle = std::move(title);
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SetWindowTextW(control, detail::toWide(mTitle).c_str());
  }
}

void NativeButton::setEnabled(const bool enabled) {
  mEnabled = enabled;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::EnableWindow(control, enabled ? TRUE : FALSE);
  }
}

bool NativeButton::enabled() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? ::IsWindowEnabled(control) != FALSE : mEnabled;
}

void NativeButton::setControlSize(const NativeControlSize size) {
  mControlSize = size;
}

void NativeButton::onWindowChanged(Window* previous, Window* current) {
  NativeViewLayer::onWindowChanged(previous, current);
  if (current != nullptr) mHost->attach(current);
  else mHost->detach();
}

} // namespace coral::ui::win32
