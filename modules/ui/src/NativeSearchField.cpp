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

#include <ui/macos/NativeSearchField.h>

#include "NativeSearchFieldInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

#include <utility>

namespace coral::ui::macos {

NativeSearchField::NativeSearchField(void* nativeView, std::string placeholder)
    : NativeViewLayer(nativeView), mPlaceholder(std::move(placeholder)) {}

NativeSearchField::~NativeSearchField() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativeSearchField> NativeSearchField::create(std::string placeholder,
    ChangeHandler onChange) {
  void* view = createNativeSearchFieldView(placeholder, std::move(onChange));
  if (view == nullptr) return {};
  auto field = std::shared_ptr<NativeSearchField>(
    new NativeSearchField(view, std::move(placeholder)));
  applyNativeViewPreferredSize(*field, view);
  return field;
}

void NativeSearchField::setText(std::string text) {
  if (nativeView() != nullptr) setNativeSearchFieldText(nativeView(), text);
}

std::string NativeSearchField::text() const {
  return nativeView() != nullptr ? nativeSearchFieldText(nativeView()) : std::string();
}

void NativeSearchField::setPlaceholder(std::string placeholder) {
  mPlaceholder = std::move(placeholder);
  if (nativeView() != nullptr) setNativeSearchFieldPlaceholder(nativeView(), mPlaceholder);
}

void NativeSearchField::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeViewControlSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativeSearchField::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeViewControlSize(nativeView()));
}

void NativeSearchField::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativeSearchField::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
