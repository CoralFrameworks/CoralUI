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

#ifndef CORAL_UI_NATIVESWITCH_H
#define CORAL_UI_NATIVESWITCH_H

#include <ui/NativeViewLayer.h>
#include <ui/macos/NativeControlSize.h>

#include <functional>
#include <memory>
#include <string>

namespace coral::ui::macos {

/** A native NSSwitchButton/NSSwitch hosted in the CoralUI layer tree. */
class NativeSwitch final : public NativeViewLayer {
 public:
  using ChangeHandler = std::function<void(bool)>;

  static std::shared_ptr<NativeSwitch> create(std::string title, bool on = false,
    ChangeHandler onChange = {});
  ~NativeSwitch() override;

  void setOn(bool on);
  [[nodiscard]] bool isOn() const;
  void setTitle(std::string title);
  [[nodiscard]] const std::string& title() const noexcept {
    return mTitle;
  }

  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const;

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

 private:
  NativeSwitch(void* nativeView, std::string title);
  std::string mTitle;
};

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVESWITCH_H
