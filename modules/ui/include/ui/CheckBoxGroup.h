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

#ifndef CORAL_UI_CHECKBOXGROUP_H
#define CORAL_UI_CHECKBOXGROUP_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;
class CheckBox;

/** A vertical multi-selection group. */
class CheckBoxGroup final : public Layer {
 public:
  CheckBoxGroup(std::vector<std::string> options, std::shared_ptr<const Font> font);
  ~CheckBoxGroup() override;

  void setOptions(std::vector<std::string> options);
  [[nodiscard]] const std::vector<std::string>& options() const noexcept {
    return mOptions;
  }
  void setSelectedIndices(std::vector<int> indices);
  [[nodiscard]] const std::vector<int>& selectedIndices() const noexcept {
    return mSelected;
  }
  void setOnChanged(std::function<void(std::vector<int>)> callback) {
    mOnChanged = std::move(callback);
  }

  Size measure(const Size& available) override;
  void layout() override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "checkboxgroup";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void rebuild();
  void toggle(int index);

  std::vector<std::string> mOptions;
  std::vector<std::shared_ptr<CheckBox>> mButtons;
  std::function<void(std::vector<int>)> mOnChanged;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<bool> mAlive = std::make_shared<bool>(true);
  std::vector<int> mSelected;
  float mGap = 4.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_CHECKBOXGROUP_H
