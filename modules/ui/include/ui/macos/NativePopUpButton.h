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

#ifndef CORAL_UI_NATIVEPOPUPBUTTON_H
#define CORAL_UI_NATIVEPOPUPBUTTON_H

#include <ui/NativeViewLayer.h>
#include <ui/macos/NativeControlSize.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui::macos {

/** A native NSPopUpButton hosted in the CoralUI layer tree; null on unsupported platforms. */
class NativePopUpButton final : public NativeViewLayer {
 public:
  using SelectionHandler = std::function<void(int, std::string_view)>;

  static std::shared_ptr<NativePopUpButton> create(std::vector<std::string> items,
    int selected = 0, SelectionHandler onSelect = {});
  ~NativePopUpButton() override;

  void setItems(std::vector<std::string> items, int selected = 0);
  [[nodiscard]] int itemCount() const;
  [[nodiscard]] int selectedIndex() const;
  void setSelectedIndex(int index);
  [[nodiscard]] std::string itemTitle(int index) const;

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const;

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

 private:
  NativePopUpButton(void* nativeView, std::vector<std::string> items, int selected);
  std::vector<std::string> mItems;
};

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVEPOPUPBUTTON_H
