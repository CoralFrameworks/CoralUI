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

#include <ui/macos/NativeColorWell.h>

#include "NativeColorWellInternal.h"
#include "NativeControlSizeInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include <utility>

namespace coral::ui::macos {

NativeColorWell::NativeColorWell(void* nativeView) : NativeViewLayer(nativeView) {}

NativeColorWell::~NativeColorWell() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeColorWell> NativeColorWell::create(const graphics::Color initial,
    ChangeHandler onChange) {
  void* view = createNativeColorWellView(initial, std::move(onChange));
  if (view == nullptr) return {};
  auto well = std::shared_ptr<NativeColorWell>(new NativeColorWell(view));
  applyNativeViewPreferredSize(*well, view);
  return well;
}

void NativeColorWell::setColor(const graphics::Color color) {
  if (nativeView() != nullptr) setNativeColorWellColor(nativeView(), color);
}

void NativeColorWell::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeColorWell::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

void NativeColorWell::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeViewControlSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeColorWell::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeViewControlSize(nativeView()));
}

graphics::Color NativeColorWell::color() const {
  return nativeView() != nullptr ? nativeColorWellColor(nativeView()) : graphics::Color{};
}

} // namespace coral::ui::macos
