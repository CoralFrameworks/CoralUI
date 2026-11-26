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

#include <ui/win32/NativeDatePicker.h>

#include "NativeControlWin32.h"

#include <utility>

namespace coral::ui::win32 {

namespace {

SYSTEMTIME toSystemTime(const std::chrono::year_month_day date) {
  SYSTEMTIME time{};
  time.wYear = static_cast<WORD>(static_cast<int>(date.year()));
  time.wMonth = static_cast<WORD>(static_cast<unsigned>(date.month()));
  time.wDay = static_cast<WORD>(static_cast<unsigned>(date.day()));
  return time;
}

std::chrono::year_month_day fromSystemTime(const SYSTEMTIME& time) {
  return std::chrono::year{ static_cast<int>(time.wYear) } /
         std::chrono::month{ static_cast<unsigned>(time.wMonth) } /
         std::chrono::day{ static_cast<unsigned>(time.wDay) };
}

} // namespace

NativeDatePicker::NativeDatePicker(void* nativeView, const int controlId,
    const std::chrono::year_month_day initial, ChangeHandler onChange)
    : NativeViewLayer(nativeView),
      mHost(std::make_shared<detail::NativeControlHost>(static_cast<HWND>(nativeView), controlId)),
      mOnChange(std::move(onChange)), mDate(initial) {
  mHost->setHandlers({}, [this](const NMHDR& header) {
    if (header.code != DTN_DATETIMECHANGE) return false;
    const auto* change = reinterpret_cast<const NMDATETIMECHANGE*>(&header);
    if (change->dwFlags != GDT_VALID) return true;
    mDate = fromSystemTime(change->st);
    if (mOnChange) mOnChange(mDate);
    return true;
  });
  setDate(mDate);
}

NativeDatePicker::~NativeDatePicker() = default;

std::shared_ptr<NativeDatePicker> NativeDatePicker::create(
    const std::chrono::year_month_day initial, ChangeHandler onChange) {
  const int controlId = detail::nextControlId();
  HWND control = detail::createControl(DATETIMEPICK_CLASSW,
    WS_CHILD | WS_CLIPSIBLINGS | DTS_SHORTDATEFORMAT, 0, controlId);
  if (control == nullptr) return {};
  auto native = std::shared_ptr<NativeDatePicker>(
    new NativeDatePicker(control, controlId, initial, std::move(onChange)));
  native->setPreferredSize(Size{ 140.0f, 28.0f });
  return native;
}

void NativeDatePicker::setDate(const std::chrono::year_month_day date) {
  mDate = date;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    const SYSTEMTIME time = toSystemTime(date);
    ::SendMessageW(control, DTM_SETSYSTEMTIME, GDT_VALID,
      reinterpret_cast<LPARAM>(&time));
  }
}

void NativeDatePicker::setEnabled(const bool enabled) {
  mEnabled = enabled;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::EnableWindow(control, enabled ? TRUE : FALSE);
  }
}

bool NativeDatePicker::enabled() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? ::IsWindowEnabled(control) != FALSE : mEnabled;
}

void NativeDatePicker::setControlSize(const NativeControlSize size) {
  mControlSize = size;
}

void NativeDatePicker::onWindowChanged(Window* previous, Window* current) {
  NativeViewLayer::onWindowChanged(previous, current);
  if (current != nullptr) mHost->attach(current);
  else mHost->detach();
}

} // namespace coral::ui::win32
