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

#include <ui/win32/NativeSlider.h>

#include "NativeControlWin32.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui::win32 {

namespace {

int toTrackbarValue(const double value) {
  return static_cast<int>(std::lround(std::clamp(value, -2147483647.0, 2147483647.0)));
}

} // namespace

NativeSlider::NativeSlider(void* nativeView, const int controlId, const double minimum,
    const double maximum, const double value, ChangeHandler onChange)
    : NativeViewLayer(nativeView),
      mHost(std::make_shared<detail::NativeControlHost>(static_cast<HWND>(nativeView), controlId)),
      mOnChange(std::move(onChange)), mMinimum(minimum), mMaximum(maximum) {
  mHost->setHandlers({}, {}, [this](const int) {
    if (mOnChange) mOnChange(this->value());
    return true;
  });
  setRange(minimum, maximum);
  setValue(value);
}

NativeSlider::~NativeSlider() = default;

std::shared_ptr<NativeSlider> NativeSlider::create(const double minimum, const double maximum,
    const double value, ChangeHandler onChange) {
  const int controlId = detail::nextControlId();
  HWND control = detail::createControl(TRACKBAR_CLASSW,
    WS_CHILD | WS_CLIPSIBLINGS | TBS_HORZ | TBS_AUTOTICKS, 0, controlId);
  if (control == nullptr) return {};
  auto native = std::shared_ptr<NativeSlider>(
    new NativeSlider(control, controlId, minimum, maximum, value, std::move(onChange)));
  native->setPreferredSize(Size{ 160.0f, 28.0f });
  return native;
}

void NativeSlider::setRange(const double minimum, const double maximum) {
  mMinimum = minimum;
  mMaximum = std::max(minimum, maximum);
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SendMessageW(control, TBM_SETRANGEMIN, FALSE, toTrackbarValue(mMinimum));
    ::SendMessageW(control, TBM_SETRANGEMAX, FALSE, toTrackbarValue(mMaximum));
  }
  setValue(value());
}

void NativeSlider::setValue(const double value) {
  const double clamped = std::clamp(value, mMinimum, mMaximum);
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SendMessageW(control, TBM_SETPOS, TRUE, toTrackbarValue(clamped));
  }
}

double NativeSlider::value() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? static_cast<double>(::SendMessageW(control, TBM_GETPOS, 0, 0)) : 0.0;
}

void NativeSlider::setContinuous(const bool continuous) {
  mContinuous = continuous;
}

void NativeSlider::setEnabled(const bool enabled) {
  mEnabled = enabled;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::EnableWindow(control, enabled ? TRUE : FALSE);
  }
}

bool NativeSlider::enabled() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? ::IsWindowEnabled(control) != FALSE : mEnabled;
}

void NativeSlider::setControlSize(const NativeControlSize size) {
  mControlSize = size;
}

void NativeSlider::onWindowChanged(Window* previous, Window* current) {
  NativeViewLayer::onWindowChanged(previous, current);
  if (current != nullptr) mHost->attach(current);
  else mHost->detach();
}

} // namespace coral::ui::win32
