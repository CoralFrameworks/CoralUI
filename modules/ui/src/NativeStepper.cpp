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

#include <ui/macos/NativeStepper.h>

#include "NativeStepperInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

#include <utility>

namespace coral::ui::macos {

NativeStepper::NativeStepper(void* nativeView) : NativeViewLayer(nativeView) {}

NativeStepper::~NativeStepper() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeStepper> NativeStepper::create(const double minimum,
    const double maximum, const double value, ChangeHandler onChange) {
  void* view = createNativeStepperView(minimum, maximum, value, std::move(onChange));
  if (view == nullptr) return {};
  auto stepper = std::shared_ptr<NativeStepper>(new NativeStepper(view));
  stepper->setPreferredSize({ 19.0f, 27.0f });
  return stepper;
}

void NativeStepper::setRange(const double minimum, const double maximum) {
  if (nativeView() != nullptr) setNativeStepperRange(nativeView(), minimum, maximum);
}

void NativeStepper::setValue(const double value) {
  if (nativeView() != nullptr) setNativeStepperValue(nativeView(), value);
}

double NativeStepper::value() const {
  return nativeView() != nullptr ? nativeStepperValue(nativeView()) : 0.0;
}

void NativeStepper::setIncrement(const double increment) {
  if (nativeView() != nullptr) setNativeStepperIncrement(nativeView(), increment);
}

double NativeStepper::increment() const {
  return nativeView() != nullptr ? nativeStepperIncrement(nativeView()) : 0.0;
}

void NativeStepper::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeStepperSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeStepper::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeStepperSize(nativeView()));
}

void NativeStepper::setValueWraps(const bool wraps) {
  if (nativeView() != nullptr) setNativeStepperValueWraps(nativeView(), wraps);
}

void NativeStepper::setAutorepeat(const bool autorepeat) {
  if (nativeView() != nullptr) setNativeStepperAutorepeat(nativeView(), autorepeat);
}

void NativeStepper::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeStepper::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
