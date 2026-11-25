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

#include <ui/RadioGroup.h>

#include <ui/RadioButton.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

RadioGroup::RadioGroup(std::vector<std::string> options, std::shared_ptr<const Font> font,
    const int selected)
    : mOptions(std::move(options)), mFont(std::move(font)),
      mSelected(std::clamp(selected, 0, std::max(0, static_cast<int>(mOptions.size()) - 1))) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Radio group");
  rebuild();
}

void RadioGroup::setOptions(std::vector<std::string> options) {
  mOptions = std::move(options);
  mSelected = std::clamp(mSelected, 0, std::max(0, static_cast<int>(mOptions.size()) - 1));
  rebuild();
}

void RadioGroup::setSelectedIndex(const int index) {
  if (mOptions.empty()) return;
  choose(std::clamp(index, 0, static_cast<int>(mOptions.size()) - 1));
}

Size RadioGroup::measure(const Size& available) {
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

void RadioGroup::layout() {
  float top = bounds().getTop();
  for (const auto& button: mButtons) {
    const Size size = button->measure({});
    button->setBounds(Rect::MakeXYWH(bounds().getLeft(), top, bounds().getWidth(), size.height));
    button->layout();
    top += size.height + mGap;
  }
  clearNeedsLayout();
}

void RadioGroup::applyTheme(const Theme& theme) {
  mGap = theme.spacing.xs;
  setNeedsLayout();
}

void RadioGroup::rebuild() {
  for (const auto& button: mButtons) removeChild(button);
  mButtons.clear();
  for (size_t index = 0; index < mOptions.size(); ++index) {
    auto button = std::make_shared<RadioButton>(mOptions[index], mFont,
      static_cast<int>(index) == mSelected);
    button->setOnSelected([this, index] { choose(static_cast<int>(index)); });
    addChild(button);
    mButtons.push_back(std::move(button));
  }
  setAccessibleValue(mSelected >= 0 && mSelected < static_cast<int>(mOptions.size())
    ? mOptions[static_cast<size_t>(mSelected)] : std::string());
  setNeedsLayout();
}

void RadioGroup::choose(const int index) {
  if (index < 0 || index >= static_cast<int>(mButtons.size())) return;
  if (index == mSelected) return;
  mSelected = index;
  for (size_t buttonIndex = 0; buttonIndex < mButtons.size(); ++buttonIndex) {
    mButtons[buttonIndex]->setSelected(static_cast<int>(buttonIndex) == mSelected);
  }
  setAccessibleValue(mOptions[static_cast<size_t>(mSelected)]);
  if (mOnChanged) mOnChanged(mSelected);
}

} // namespace coral::ui
