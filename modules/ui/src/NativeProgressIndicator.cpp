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

#include <ui/macos/NativeProgressIndicator.h>

#include "NativeProgressIndicatorInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

namespace coral::ui::macos {

NativeProgressIndicator::NativeProgressIndicator(void* nativeView) : NativeViewLayer(nativeView) {}

NativeProgressIndicator::~NativeProgressIndicator() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeProgressIndicator> NativeProgressIndicator::create(
    const bool indeterminate, const NativeProgressStyle style) {
  void* view = createNativeProgressIndicatorView(indeterminate,
    style == NativeProgressStyle::Spinning);
  if (view == nullptr) return {};
  auto indicator = std::shared_ptr<NativeProgressIndicator>(new NativeProgressIndicator(view));
  indicator->setPreferredSize({ 160.0f, 12.0f });
  return indicator;
}

void NativeProgressIndicator::setIndeterminate(const bool indeterminate) {
  if (nativeView() != nullptr) {
    setNativeProgressIndicatorIndeterminate(nativeView(), indeterminate);
  }
}

bool NativeProgressIndicator::indeterminate() const {
  return nativeView() != nullptr && nativeProgressIndicatorIndeterminate(nativeView());
}

void NativeProgressIndicator::setValue(const double value) {
  if (nativeView() != nullptr) setNativeProgressIndicatorValue(nativeView(), value);
}

double NativeProgressIndicator::value() const {
  return nativeView() != nullptr ? nativeProgressIndicatorValue(nativeView()) : 0.0;
}

void NativeProgressIndicator::setStyle(const NativeProgressStyle style) {
  if (nativeView() != nullptr) {
    setNativeProgressIndicatorStyle(nativeView(), style == NativeProgressStyle::Spinning);
  }
}

void NativeProgressIndicator::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeProgressIndicatorSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeProgressIndicator::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeProgressIndicatorSize(nativeView()));
}

void NativeProgressIndicator::setDisplayedWhenStopped(const bool displayed) {
  if (nativeView() != nullptr) {
    setNativeProgressIndicatorDisplayedWhenStopped(nativeView(), displayed);
  }
}

bool NativeProgressIndicator::displayedWhenStopped() const {
  return nativeView() != nullptr && nativeProgressIndicatorDisplayedWhenStopped(nativeView());
}

NativeProgressStyle NativeProgressIndicator::style() const {
  return nativeView() != nullptr && nativeProgressIndicatorStyle(nativeView())
    ? NativeProgressStyle::Spinning : NativeProgressStyle::Bar;
}

void NativeProgressIndicator::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeProgressIndicator::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
