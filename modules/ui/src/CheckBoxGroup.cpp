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

#include <ui/CheckBoxGroup.h>

#include <ui/CheckBox.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

CheckBoxGroup::CheckBoxGroup(std::vector<std::string> options,
    std::shared_ptr<const Font> font)
    : mOptions(std::move(options)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Checkbox group");
  rebuild();
}

CheckBoxGroup::~CheckBoxGroup() { *mAlive = false; }

void CheckBoxGroup::setOptions(std::vector<std::string> options) {
  mOptions = std::move(options);
  mSelected.erase(std::remove_if(mSelected.begin(), mSelected.end(), [this](const int index) {
    return index < 0 || index >= static_cast<int>(mOptions.size());
  }), mSelected.end());
  rebuild();
}

void CheckBoxGroup::setSelectedIndices(std::vector<int> indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  indices.erase(std::remove_if(indices.begin(), indices.end(), [this](const int index) {
    return index < 0 || index >= static_cast<int>(mButtons.size());
  }), indices.end());
  mSelected = std::move(indices);
  for (size_t index = 0; index < mButtons.size(); ++index) {
    mButtons[index]->setChecked(std::find(mSelected.begin(), mSelected.end(),
      static_cast<int>(index)) != mSelected.end());
  }
  setAccessibleValue(std::to_string(mSelected.size()));
  if (mOnChanged) mOnChanged(mSelected);
}

Size CheckBoxGroup::measure(const Size& available) {
  (void)available;
  float width = 80.0f;
  float height = 0.0f;
  for (const auto& button: mButtons) {
    const Size size = button->measure({});
    width = std::max(width, size.width);
    height += size.height + mGap;
  }
  if (height > 0.0f) height -= mGap;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void CheckBoxGroup::layout() {
  float top = bounds().getTop();
  for (const auto& button: mButtons) {
    const Size size = button->measure({});
    button->setBounds(Rect::MakeXYWH(bounds().getLeft(), top, bounds().getWidth(), size.height));
    button->layout();
    top += size.height + mGap;
  }
  clearNeedsLayout();
}

void CheckBoxGroup::applyTheme(const Theme& theme) {
  mGap = theme.spacing.xs;
  setNeedsLayout();
}

void CheckBoxGroup::rebuild() {
  for (const auto& button: mButtons) removeChild(button);
  mButtons.clear();
  const auto alive = mAlive;
  for (size_t index = 0; index < mOptions.size(); ++index) {
    const bool checked = std::find(mSelected.begin(), mSelected.end(), static_cast<int>(index)) !=
      mSelected.end();
    auto button = std::make_shared<CheckBox>(mOptions[index], mFont, checked);
    button->setOnChanged([this, alive, index](const bool on) {
      if (!*alive) return;
      if (on) {
        mSelected.push_back(static_cast<int>(index));
        std::sort(mSelected.begin(), mSelected.end());
      } else {
        mSelected.erase(std::remove(mSelected.begin(), mSelected.end(),
          static_cast<int>(index)), mSelected.end());
      }
      setAccessibleValue(std::to_string(mSelected.size()));
      if (mOnChanged) mOnChanged(mSelected);
    });
    addChild(button);
    mButtons.push_back(std::move(button));
  }
  setAccessibleValue(std::to_string(mSelected.size()));
  setNeedsLayout();
}

void CheckBoxGroup::toggle(const int index) {
  setSelectedIndices({ index });
}

} // namespace coral::ui
