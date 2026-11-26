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

#include <ui/win32/NativePopUpButton.h>

#include "NativeControlWin32.h"

#include <algorithm>
#include <utility>

namespace coral::ui::win32 {

NativePopUpButton::NativePopUpButton(void* nativeView, const int controlId,
    std::vector<std::string> items, const int selected, SelectionHandler onSelect)
    : NativeViewLayer(nativeView),
      mHost(std::make_shared<detail::NativeControlHost>(static_cast<HWND>(nativeView), controlId)),
      mOnSelect(std::move(onSelect)), mItems(std::move(items)) {
  mHost->setHandlers([this](const int notificationCode) {
    if (notificationCode == CBN_SELCHANGE) {
      const int index = selectedIndex();
      if (mOnSelect) mOnSelect(index, itemTitle(index));
      return true;
    }
    return false;
  });
  setItems(mItems, selected);
}

NativePopUpButton::~NativePopUpButton() = default;

std::shared_ptr<NativePopUpButton> NativePopUpButton::create(std::vector<std::string> items,
    const int selected, SelectionHandler onSelect) {
  const int controlId = detail::nextControlId();
  HWND control = detail::createControl(L"COMBOBOX",
    WS_CHILD | WS_CLIPSIBLINGS | CBS_DROPDOWNLIST | WS_VSCROLL, 0, controlId);
  if (control == nullptr) return {};
  auto native = std::shared_ptr<NativePopUpButton>(
    new NativePopUpButton(control, controlId, std::move(items), selected, std::move(onSelect)));
  ::SendMessageW(control, CB_SETMINVISIBLE, 8, 0);
  native->setPreferredSize(Size{ 140.0f, 28.0f });
  return native;
}

void NativePopUpButton::setItems(std::vector<std::string> items, const int selected) {
  mItems = std::move(items);
  HWND control = static_cast<HWND>(nativeView());
  if (control == nullptr) return;
  ::SendMessageW(control, CB_RESETCONTENT, 0, 0);
  for (const std::string& item: mItems) {
    ::SendMessageW(control, CB_ADDSTRING, 0,
      reinterpret_cast<LPARAM>(detail::toWide(item).c_str()));
  }
  setSelectedIndex(selected);
}

int NativePopUpButton::itemCount() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? static_cast<int>(::SendMessageW(control, CB_GETCOUNT, 0, 0)) : 0;
}

int NativePopUpButton::selectedIndex() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? static_cast<int>(::SendMessageW(control, CB_GETCURSEL, 0, 0)) : -1;
}

void NativePopUpButton::setSelectedIndex(const int index) {
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    const int count = itemCount();
    if (count <= 0) return;
    ::SendMessageW(control, CB_SETCURSEL,
      static_cast<WPARAM>(std::clamp(index, 0, count - 1)), 0);
  }
}

std::string NativePopUpButton::itemTitle(const int index) const {
  HWND control = static_cast<HWND>(nativeView());
  if (control == nullptr || index < 0) return {};
  const LRESULT length = ::SendMessageW(control, CB_GETLBTEXTLEN, static_cast<WPARAM>(index), 0);
  if (length < 0) return {};
  std::wstring text(static_cast<size_t>(length) + 1, L'\0');
  ::SendMessageW(control, CB_GETLBTEXT, static_cast<WPARAM>(index),
    reinterpret_cast<LPARAM>(text.data()));
  text.resize(static_cast<size_t>(length));
  return detail::toUtf8(text);
}

void NativePopUpButton::setEnabled(const bool enabled) {
  mEnabled = enabled;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::EnableWindow(control, enabled ? TRUE : FALSE);
  }
}

bool NativePopUpButton::enabled() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? ::IsWindowEnabled(control) != FALSE : mEnabled;
}

void NativePopUpButton::setControlSize(const NativeControlSize size) {
  mControlSize = size;
}

void NativePopUpButton::onWindowChanged(Window* previous, Window* current) {
  NativeViewLayer::onWindowChanged(previous, current);
  if (current != nullptr) mHost->attach(current);
  else mHost->detach();
}

} // namespace coral::ui::win32
