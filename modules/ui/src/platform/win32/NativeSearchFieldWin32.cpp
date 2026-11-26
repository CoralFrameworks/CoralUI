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

#include <ui/win32/NativeSearchField.h>

#include "NativeControlWin32.h"

#include <utility>

namespace coral::ui::win32 {

NativeSearchField::NativeSearchField(void* nativeView, const int controlId, std::string placeholder,
    ChangeHandler onChange)
    : NativeViewLayer(nativeView),
      mHost(std::make_shared<detail::NativeControlHost>(static_cast<HWND>(nativeView), controlId)),
      mOnChange(std::move(onChange)), mPlaceholder(std::move(placeholder)) {
  mHost->setHandlers([this](const int notificationCode) {
    if (notificationCode == EN_CHANGE && !mSettingText) {
      if (mOnChange) mOnChange(text());
      return true;
    }
    return false;
  });
  setPlaceholder(mPlaceholder);
}

NativeSearchField::~NativeSearchField() = default;

std::shared_ptr<NativeSearchField> NativeSearchField::create(std::string placeholder,
    ChangeHandler onChange) {
  const int controlId = detail::nextControlId();
  HWND control = detail::createControl(L"EDIT",
    WS_CHILD | WS_CLIPSIBLINGS | ES_AUTOHSCROLL | ES_LEFT, WS_EX_CLIENTEDGE, controlId);
  if (control == nullptr) return {};
  auto native = std::shared_ptr<NativeSearchField>(
    new NativeSearchField(control, controlId, std::move(placeholder), std::move(onChange)));
  native->setPreferredSize(Size{ 220.0f, 28.0f });
  return native;
}

void NativeSearchField::setText(std::string text) {
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    mSettingText = true;
    ::SetWindowTextW(control, detail::toWide(text).c_str());
    mSettingText = false;
  }
}

std::string NativeSearchField::text() const {
  HWND control = static_cast<HWND>(nativeView());
  if (control == nullptr) return {};
  const int length = ::GetWindowTextLengthW(control);
  if (length <= 0) return {};
  std::wstring wide(static_cast<size_t>(length) + 1, L'\0');
  ::GetWindowTextW(control, wide.data(), length + 1);
  wide.resize(static_cast<size_t>(length));
  return detail::toUtf8(wide);
}

void NativeSearchField::setPlaceholder(std::string placeholder) {
  mPlaceholder = std::move(placeholder);
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SendMessageW(control, EM_SETCUEBANNER, TRUE,
      reinterpret_cast<LPARAM>(detail::toWide(mPlaceholder).c_str()));
  }
}

void NativeSearchField::setEnabled(const bool enabled) {
  mEnabled = enabled;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::EnableWindow(control, enabled ? TRUE : FALSE);
  }
}

bool NativeSearchField::enabled() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? ::IsWindowEnabled(control) != FALSE : mEnabled;
}

void NativeSearchField::setControlSize(const NativeControlSize size) {
  mControlSize = size;
}

void NativeSearchField::onWindowChanged(Window* previous, Window* current) {
  NativeViewLayer::onWindowChanged(previous, current);
  if (current != nullptr) mHost->attach(current);
  else mHost->detach();
}

} // namespace coral::ui::win32
