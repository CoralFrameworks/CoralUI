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

#include <ui/macos/NativeButton.h>

#include "NativeButtonInternal.h"
#include "NativeControlSizeInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

#include <utility>

namespace coral::ui::macos {

NativeButton::NativeButton(void* nativeView, std::string title)
    : NativeViewLayer(nativeView), mTitle(std::move(title)) {}

NativeButton::~NativeButton() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeButton> NativeButton::create(std::string title, ClickHandler onClick) {
  void* view = createNativeButtonView(title, std::move(onClick));
  if (view == nullptr) return {};
  auto button = std::shared_ptr<NativeButton>(new NativeButton(view, std::move(title)));
  applyNativeViewPreferredSize(*button, view);
  return button;
}

void NativeButton::setTitle(std::string title) {
  mTitle = std::move(title);
  if (nativeView() != nullptr) setNativeButtonTitle(nativeView(), mTitle);
}

void NativeButton::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeButtonSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeButton::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeButtonSize(nativeView()));
}

void NativeButton::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeButton::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
