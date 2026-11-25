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

#ifndef CORAL_UI_RADIOGROUP_H
#define CORAL_UI_RADIOGROUP_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class RadioButton;

/** A vertical, exclusive set of RadioButtons. */
class RadioGroup final : public Layer {
 public:
  RadioGroup(std::vector<std::string> options, std::shared_ptr<const Font> font,
    int selected = 0);

  void setOptions(std::vector<std::string> options);
  [[nodiscard]] const std::vector<std::string>& options() const noexcept {
    return mOptions;
  }
  void setSelectedIndex(int index);
  [[nodiscard]] int selectedIndex() const noexcept {
    return mSelected;
  }
  void setOnChanged(std::function<void(int)> callback) {
    mOnChanged = std::move(callback);
  }

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "radiogroup";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void rebuild();
  void choose(int index);

  std::vector<std::string> mOptions;
  std::vector<std::shared_ptr<RadioButton>> mButtons;
  std::shared_ptr<const Font> mFont;
  std::function<void(int)> mOnChanged;
  int mSelected = 0;
  float mGap = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_RADIOGROUP_H
