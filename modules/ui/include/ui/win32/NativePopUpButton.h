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

#ifndef CORAL_UI_WIN32_NATIVEPOPUPBUTTON_H
#define CORAL_UI_WIN32_NATIVEPOPUPBUTTON_H

#include <ui/NativeViewLayer.h>
#include <ui/win32/NativeControlSize.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace coral::ui::win32 {

namespace detail {
class NativeControlHost;
}

/** A native Win32 combo box (`COMBOBOX`, drop-down list) hosted in the CoralUI layer tree. */
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

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const noexcept {
    return mControlSize;
  }

  void onWindowChanged(Window* previous, Window* current) override;

 private:
  NativePopUpButton(void* nativeView, int controlId, std::vector<std::string> items, int selected,
    SelectionHandler onSelect);

  std::shared_ptr<detail::NativeControlHost> mHost;
  SelectionHandler mOnSelect;
  std::vector<std::string> mItems;
  bool mEnabled = true;
  NativeControlSize mControlSize = NativeControlSize::Regular;
};

} // namespace coral::ui::win32

#endif // CORAL_UI_WIN32_NATIVEPOPUPBUTTON_H
