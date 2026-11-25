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

#include <ui/macos/NativeSegmentedControl.h>

#include "NativeSegmentedControlInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

#include <utility>

namespace coral::ui::macos {

NativeSegmentedControl::NativeSegmentedControl(void* nativeView) : NativeViewLayer(nativeView) {}

NativeSegmentedControl::~NativeSegmentedControl() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeSegmentedControl> NativeSegmentedControl::create(
    std::vector<std::string> labels, const int selected, SelectionHandler onSelect) {
  void* view = createNativeSegmentedControlView(std::move(labels), selected,
    std::move(onSelect));
  if (view == nullptr) return {};
  auto control = std::shared_ptr<NativeSegmentedControl>(new NativeSegmentedControl(view));
  applyNativeViewPreferredSize(*control, view);
  return control;
}

void NativeSegmentedControl::setLabels(std::vector<std::string> labels) {
  if (nativeView() != nullptr) {
    setNativeSegmentedControlLabels(nativeView(), std::move(labels));
  }
}

int NativeSegmentedControl::segmentCount() const {
  return nativeView() != nullptr ? nativeSegmentedControlCount(nativeView()) : 0;
}

int NativeSegmentedControl::selectedIndex() const {
  return nativeView() != nullptr ? nativeSegmentedControlSelectedIndex(nativeView()) : -1;
}

void NativeSegmentedControl::setSelectedIndex(const int index) {
  if (nativeView() != nullptr) setNativeSegmentedControlSelectedIndex(nativeView(), index);
}

void NativeSegmentedControl::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeViewControlSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeSegmentedControl::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeViewControlSize(nativeView()));
}

void NativeSegmentedControl::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeSegmentedControl::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
