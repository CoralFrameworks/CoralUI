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

#ifndef CORAL_UI_TOASTSTACK_H
#define CORAL_UI_TOASTSTACK_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

struct Theme;
class Toast;

/** A bounded stack of transient Toast layers. */
class ToastStack final : public Layer {
 public:
  ToastStack(std::shared_ptr<const Font> font, int maxVisible = 3);

  void setMaxVisible(int count);
  [[nodiscard]] int maxVisible() const noexcept {
    return mMaxVisible;
  }
  [[nodiscard]] int count() const noexcept {
    return static_cast<int>(mToasts.size());
  }
  void show(std::string text,
    std::chrono::milliseconds duration = std::chrono::milliseconds(2500));
  void clear();

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "toaststack";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  std::shared_ptr<const Font> mFont;
  std::vector<std::shared_ptr<Toast>> mToasts;
  std::shared_ptr<bool> mAlive = std::make_shared<bool>(true);
  int mMaxVisible = 3;
  float mGap = 8.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_TOASTSTACK_H
