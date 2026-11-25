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

#include <ui/macos/NativePopUpButton.h>

#include "NativePopUpButtonInternal.h"

#import <CoreFoundation/CoreFoundation.h>

#include "NativeControlSizeInternal.h"

#include <algorithm>
#include <utility>

namespace coral::ui::macos {

NativePopUpButton::NativePopUpButton(void* nativeView, std::vector<std::string> items,
    const int selected) : NativeViewLayer(nativeView), mItems(std::move(items)) {
  (void)selected;
}

NativePopUpButton::~NativePopUpButton() {
  if (nativeView() != nullptr) CFRelease((CFTypeRef)nativeView());
}

std::shared_ptr<NativePopUpButton> NativePopUpButton::create(std::vector<std::string> items,
    const int selected, SelectionHandler onSelect) {
  void* view = createNativePopUpButtonView(items, selected, std::move(onSelect));
  if (view == nullptr) return {};
  auto button = std::shared_ptr<NativePopUpButton>(
    new NativePopUpButton(view, std::move(items), selected));
  applyNativeViewPreferredSize(*button, view);
  return button;
}

void NativePopUpButton::setItems(std::vector<std::string> items, const int selected) {
  mItems = std::move(items);
  if (nativeView() != nullptr) setNativePopUpButtonItems(nativeView(), mItems, selected);
}

int NativePopUpButton::itemCount() const {
  return nativeView() != nullptr ? nativePopUpButtonCount(nativeView())
                                 : static_cast<int>(mItems.size());
}

int NativePopUpButton::selectedIndex() const {
  return nativeView() != nullptr ? nativePopUpButtonSelectedIndex(nativeView()) : -1;
}

void NativePopUpButton::setSelectedIndex(const int index) {
  if (nativeView() != nullptr) setNativePopUpButtonSelectedIndex(nativeView(), index);
}

std::string NativePopUpButton::itemTitle(const int index) const {
  if (nativeView() != nullptr) return nativePopUpButtonItemTitle(nativeView(), index);
  return index >= 0 && index < static_cast<int>(mItems.size())
    ? mItems[static_cast<size_t>(index)] : std::string();
}

void NativePopUpButton::setControlSize(const NativeControlSize size) {
  if (nativeView() != nullptr) {
    setNativeViewControlSize(nativeView(), static_cast<int>(size));
    applyNativeViewPreferredSize(*this, nativeView());
  }
}

NativeControlSize NativePopUpButton::controlSize() const {
  if (nativeView() == nullptr) return NativeControlSize::Regular;
  return static_cast<NativeControlSize>(nativeViewControlSize(nativeView()));
}

void NativePopUpButton::setEnabled(const bool enabled) {
  if (nativeView() != nullptr) setNativeViewEnabled(nativeView(), enabled);
}

bool NativePopUpButton::enabled() const {
  return nativeView() != nullptr && nativeViewEnabled(nativeView());
}

} // namespace coral::ui::macos
