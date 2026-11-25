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

#ifndef CORAL_UI_NATIVEBUTTON_H
#define CORAL_UI_NATIVEBUTTON_H

#include <ui/NativeViewLayer.h>
#include <ui/macos/NativeControlSize.h>

#include <functional>
#include <memory>
#include <string>

namespace coral::ui::macos {

/** A native NSButton hosted in the CoralUI layer tree; null on platforms without native buttons. */
class NativeButton final : public NativeViewLayer {
 public:
  using ClickHandler = std::function<void()>;

  static std::shared_ptr<NativeButton> create(std::string title, ClickHandler onClick = {});
  ~NativeButton() override;

  void setTitle(std::string title);
  void setControlSize(NativeControlSize size);
  [[nodiscard]] NativeControlSize controlSize() const;
  [[nodiscard]] const std::string& title() const noexcept {
    return mTitle;
  }

  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const;

 private:
  NativeButton(void* nativeView, std::string title);
  std::string mTitle;
};

} // namespace coral::ui::macos

#endif // CORAL_UI_NATIVEBUTTON_H
