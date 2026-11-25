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

#include <ui/macos/NativeSlider.h>

#include "NativeSliderInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

#include <utility>

namespace coral::ui::macos {

NativeSlider::NativeSlider(void* nativeView) : NativeViewLayer(nativeView) {}

NativeSlider::~NativeSlider() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeSlider> NativeSlider::create(const double minimum, const double maximum,
    const double value, ChangeHandler onChange) {
  void* view = createNativeSliderView(minimum, maximum, value, std::move(onChange));
  if (view == nullptr) return {};
  auto slider = std::shared_ptr<NativeSlider>(new NativeSlider(view));
  applyNativeViewPreferredSize(*slider, view);
  return slider;
}

void NativeSlider::setRange(const double minimum, const double maximum) {
  if (nativeView() != nullptr) setNativeSliderRange(nativeView(), minimum, maximum);
}

void NativeSlider::setValue(const double value) {
  if (nativeView() != nullptr) setNativeSliderValue(nativeView(), value);
}

double NativeSlider::value() const {
  return nativeView() != nullptr ? nativeSliderValue(nativeView()) : 0.0;
}

void NativeSlider::setContinuous(const bool continuous) {
  if (nativeView() != nullptr) setNativeSliderContinuous(nativeView(), continuous);
}

void NativeSlider::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeViewControlSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeSlider::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeViewControlSize(nativeView()));
}

void NativeSlider::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeSlider::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
