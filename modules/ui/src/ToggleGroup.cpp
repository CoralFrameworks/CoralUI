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

#include <ui/ToggleGroup.h>

#include <ui/Theme.h>
#include <ui/ToggleButton.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

ToggleGroup::ToggleGroup(std::vector<std::string> options,
    std::shared_ptr<const Font> font)
    : mOptions(std::move(options)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Toggle group");
  rebuild();
}

ToggleGroup::~ToggleGroup() { *mAlive = false; }

void ToggleGroup::setOptions(std::vector<std::string> options) {
  mOptions = std::move(options);
  mSelected.erase(std::remove_if(mSelected.begin(), mSelected.end(), [this](const int index) {
    return index < 0 || index >= static_cast<int>(mOptions.size());
  }), mSelected.end());
  rebuild();
}

void ToggleGroup::setSelectedIndices(std::vector<int> indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  indices.erase(std::remove_if(indices.begin(), indices.end(), [this](const int index) {
    return index < 0 || index >= static_cast<int>(mButtons.size());
  }), indices.end());
  mSelected = std::move(indices);
  mUpdating = true;
  for (size_t index = 0; index < mButtons.size(); ++index) {
    mButtons[index]->setSelected(std::find(mSelected.begin(), mSelected.end(),
      static_cast<int>(index)) != mSelected.end());
  }
  mUpdating = false;
  setAccessibleValue(std::to_string(mSelected.size()));
  if (mOnChanged) mOnChanged(mSelected);
}

Size ToggleGroup::measure(const Size& available) {
  (void)available;
  float width = 0.0f;
  float height = 0.0f;
  for (size_t index = 0; index < mButtons.size(); ++index) {
    const Size size = mButtons[index]->measure({});
    width += size.width + (index > 0 ? mGap : 0.0f);
    height = std::max(height, size.height);
  }
  setMeasuredSize({ width, height });
  return measuredSize();
}

void ToggleGroup::layout() {
  float left = bounds().getLeft();
  for (const auto& button: mButtons) {
    const Size size = button->measure({});
    button->setBounds(Rect::MakeXYWH(left, bounds().getTop(), size.width, bounds().getHeight()));
    button->layout();
    left += size.width + mGap;
  }
  clearNeedsLayout();
}

void ToggleGroup::applyTheme(const Theme& theme) {
  mGap = theme.spacing.xs;
  setNeedsLayout();
}

void ToggleGroup::rebuild() {
  for (const auto& button: mButtons) removeChild(button);
  mButtons.clear();
  const auto alive = mAlive;
  for (size_t index = 0; index < mOptions.size(); ++index) {
    const bool selected = std::find(mSelected.begin(), mSelected.end(),
      static_cast<int>(index)) != mSelected.end();
    auto button = std::make_shared<ToggleButton>(mOptions[index], mFont, selected);
    button->setOnChanged([this, alive, index](bool on) {
      if (!*alive || mUpdating) return;
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

void ToggleGroup::toggle(const int index) {
  setSelectedIndices({ index });
}

} // namespace coral::ui
