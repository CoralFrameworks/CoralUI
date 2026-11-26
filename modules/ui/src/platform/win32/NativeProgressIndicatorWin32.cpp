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

#include <ui/win32/NativeProgressIndicator.h>

#include "NativeControlWin32.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui::win32 {

NativeProgressIndicator::NativeProgressIndicator(void* nativeView, const int controlId,
    const bool indeterminate, const NativeProgressStyle style)
    : NativeViewLayer(nativeView),
      mHost(std::make_shared<detail::NativeControlHost>(static_cast<HWND>(nativeView), controlId)),
      mIndeterminate(indeterminate), mStyle(style) {
  setIndeterminate(indeterminate);
  setValue(0.0);
}

NativeProgressIndicator::~NativeProgressIndicator() = default;

std::shared_ptr<NativeProgressIndicator> NativeProgressIndicator::create(
    const bool indeterminate, const NativeProgressStyle style) {
  const int controlId = detail::nextControlId();
  // PBS_MARQUEE permanently replaces the position bar with the marquee animation, so only request
  // it for a control that starts indeterminate. A determinate control must be created without it.
  const DWORD controlStyle = WS_CHILD | WS_CLIPSIBLINGS | PBS_SMOOTH |
    (indeterminate ? PBS_MARQUEE : 0);
  HWND control = detail::createControl(PROGRESS_CLASSW, controlStyle, 0, controlId);
  if (control == nullptr) return {};
  auto native = std::shared_ptr<NativeProgressIndicator>(
    new NativeProgressIndicator(control, controlId, indeterminate, style));
  native->setPreferredSize(Size{ 160.0f, 20.0f });
  return native;
}

void NativeProgressIndicator::setIndeterminate(const bool indeterminate) {
  mIndeterminate = indeterminate;
  HWND control = static_cast<HWND>(nativeView());
  if (control == nullptr) return;
  if (indeterminate) {
    ::SendMessageW(control, PBM_SETMARQUEE, TRUE, 30);
  } else {
    ::SendMessageW(control, PBM_SETMARQUEE, FALSE, 0);
    setValue(mValue);
  }
}

void NativeProgressIndicator::setValue(const double value) {
  mValue = std::clamp(value, 0.0, 1.0);
  if (mIndeterminate) return;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::SendMessageW(control, PBM_SETPOS,
      static_cast<WPARAM>(std::lround(mValue * 100.0)), 0);
  }
}

void NativeProgressIndicator::setStyle(const NativeProgressStyle style) {
  mStyle = style;
}

void NativeProgressIndicator::setDisplayedWhenStopped(const bool displayed) {
  mDisplayedWhenStopped = displayed;
}

void NativeProgressIndicator::setEnabled(const bool enabled) {
  mEnabled = enabled;
  if (HWND control = static_cast<HWND>(nativeView()); control != nullptr) {
    ::EnableWindow(control, enabled ? TRUE : FALSE);
  }
}

bool NativeProgressIndicator::enabled() const {
  HWND control = static_cast<HWND>(nativeView());
  return control != nullptr ? ::IsWindowEnabled(control) != FALSE : mEnabled;
}

void NativeProgressIndicator::setControlSize(const NativeControlSize size) {
  mControlSize = size;
}

void NativeProgressIndicator::onWindowChanged(Window* previous, Window* current) {
  NativeViewLayer::onWindowChanged(previous, current);
  if (current != nullptr) mHost->attach(current);
  else mHost->detach();
}

} // namespace coral::ui::win32
