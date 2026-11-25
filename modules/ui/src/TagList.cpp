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

#include <ui/TagList.h>

#include <ui/Chip.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

#include <algorithm>
#include <string>
#include <utility>

namespace coral::ui {

TagList::TagList(std::vector<std::string> tags, std::shared_ptr<const Font> font,
    const int maxVisible)
    : mTags(std::move(tags)), mFont(std::move(font)), mMaxVisible(std::max(1, maxVisible)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Tags");
  setAccessibleValue(std::to_string(mTags.size()));
  rebuild();
}

void TagList::setTags(std::vector<std::string> tags) {
  mTags = std::move(tags);
  setAccessibleValue(std::to_string(mTags.size()));
  rebuild();
}

void TagList::setMaxVisible(const int count) {
  mMaxVisible = std::max(1, count);
  rebuild();
}

Size TagList::measure(const Size& available) {
  (void)available;
  float width = 0.0f;
  float height = 26.0f;
  const int visible = std::min(mMaxVisible, static_cast<int>(mTags.size()));
  for (int index = 0; index < visible; ++index) {
    const Size size = mChips[static_cast<size_t>(index)]->measure({});
    width += size.width + (index > 0 ? mGap : 0.0f);
    height = std::max(height, size.height);
  }
  if (static_cast<int>(mTags.size()) > visible) {
    width += mGap + 24.0f;
  }
  setMeasuredSize({ width, height });
  return measuredSize();
}

void TagList::layout() {
  float left = bounds().getLeft();
  const int visible = std::min(mMaxVisible, static_cast<int>(mTags.size()));
  for (int index = 0; index < visible; ++index) {
    auto& chip = mChips[static_cast<size_t>(index)];
    const Size size = chip->measure({});
    chip->setBounds(Rect::MakeXYWH(left, bounds().getTop(), size.width, size.height));
    chip->layout();
    left += size.width + mGap;
  }
  if (mMore != nullptr) {
    mMore->setBounds(Rect::MakeXYWH(left, bounds().getTop(), 24.0f, bounds().getHeight()));
    mMore->layout();
  }
  clearNeedsLayout();
}

void TagList::applyTheme(const Theme& theme) {
  if (mMore != nullptr) mMore->setColor(theme.textMuted);
  mGap = theme.spacing.xs;
  setNeedsLayout();
}

void TagList::rebuild() {
  for (const auto& chip: mChips) removeChild(chip);
  mChips.clear();
  if (mMore != nullptr) {
    removeChild(mMore);
    mMore.reset();
  }
  const int visible = std::min(mMaxVisible, static_cast<int>(mTags.size()));
  for (int index = 0; index < visible; ++index) {
    auto chip = std::make_shared<Chip>(mTags[static_cast<size_t>(index)], mFont);
    chip->setRemovable(false);
    addChild(chip);
    mChips.push_back(std::move(chip));
  }
  if (static_cast<int>(mTags.size()) > visible) {
    mMore = std::make_shared<TextLayer>("+" + std::to_string(
      static_cast<int>(mTags.size()) - visible), mFont);
    addChild(mMore);
  }
  setNeedsLayout();
}

} // namespace coral::ui
