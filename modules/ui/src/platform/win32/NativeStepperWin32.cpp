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

#include <ui/win32/NativeStepper.h>

#include "NativeControlWin32.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui::win32 {

namespace {

int toUpDownValue(const double value) {
  return static_cast<int>(std::lround(std::clamp(value, -2147483647.0, 2147483647.0)));
}

} // namespace

NativeStepper::NativeStepper(void* nativeView, const int controlId, const double minimum,
    const double maximum, const double value, ChangeHandler onChange)
    : NativeViewLayer(nativeView),
      mHost(std::make_shared<detail::NativeControlHost>(static_cast<HWND>(nativeView), controlId)),
      mOnChange(std::move(onChange)), mMinimum(minimum), mMaximum(maximum), mValue(value) {
  mHost->setHandlers({}, [this](const NMHDR& header) {
    if (header.code != UDN_DELTAPOS) return false;
    const auto* updown = reinterpret_cast<const NMUPDOWN*>(&header);
    const double next = std::clamp(mValue + static_cast<double>(updown->iDelta) * mIncrement,
      mMinimum, mMaximum);
    if (next != mValue) {
      setValue(next);
      if (mOnChange) mOnChange(next);
    }
    return true; // The wrapper owns the position; do not let the control apply a 1-step delta.
  });
  setRange(minimum, maximum);
  setValue(value);
}

NativeStepper::~NativeStepper() = default;

std::shared_ptr<NativeStepper> NativeStepper::create(const double minimum, const double maximum,
    const double value, ChangeHandler onChange) {
  const int controlId = detail::nextControlId();
  HWND control = detail::createControl(UPDOWN_CLASSW,
    WS_CHILD | WS_CLIPSIBLINGS | UDS_ARROWKEYS | UDS_ALIGNRIGHT, 0, controlId);
  if (control == nullptr) return {};
  auto native = std::shared_ptr<NativeStepper>(
    new NativeStepper(control, controlId, minimum, maximum, value, std::move(onChange)));
  native->setPreferredSize(Size{ 60.0f, 28.0f });
  return native;
}

void NativeStepper::setRange(const double minimum, const double maximum) {
  mMinimum = minimum;
  mMaximum = std::max(minimum, maximum);
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SendMessageW(control, UDM_SETRANGE32, static_cast<WPARAM>(toUpDownValue(mMinimum)),
      static_cast<LPARAM>(toUpDownValue(mMaximum)));
  }
  setValue(mValue);
}

void NativeStepper::setValue(const double value) {
  mValue = std::clamp(value, mMinimum, mMaximum);
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SendMessageW(control, UDM_SETPOS32, 0, static_cast<LPARAM>(toUpDownValue(mValue)));
  }
}

void NativeStepper::setIncrement(const double increment) {
  mIncrement = increment == 0.0 ? 1.0 : increment;
}

void NativeStepper::setAutorepeat(const bool autorepeat) {
  mAutorepeat = autorepeat;
}

void NativeStepper::setValueWraps(const bool wraps) {
  mValueWraps = wraps;
}

void NativeStepper::setEnabled(const bool enabled) {
  mEnabled = enabled;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::EnableWindow(control, enabled ? TRUE : FALSE);
  }
}

bool NativeStepper::enabled() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? ::IsWindowEnabled(control) != FALSE : mEnabled;
}

void NativeStepper::setControlSize(const NativeControlSize size) {
  mControlSize = size;
}

void NativeStepper::onWindowChanged(Window* previous, Window* current) {
  NativeViewLayer::onWindowChanged(previous, current);
  if (current != nullptr) mHost->attach(current);
  else mHost->detach();
}

} // namespace coral::ui::win32
