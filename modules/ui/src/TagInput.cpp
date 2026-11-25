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

#include <ui/TagInput.h>

#include <ui/Chip.h>
#include <ui/TextInput.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

TagInput::TagInput(std::shared_ptr<const Font> font, std::string placeholder)
    : mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Tags");
  mInput = std::make_shared<TextInput>("", mFont);
  mInput->setPlaceholder(std::move(placeholder));
  mInput->setOnSubmit([this](const std::string& text) { addToken(text); });
  addChild(mInput);
}

void TagInput::setTokens(std::vector<std::string> tokens) {
  mTokens = std::move(tokens);
  rebuild();
}

void TagInput::setPlaceholder(std::string placeholder) {
  if (mInput != nullptr) mInput->setPlaceholder(std::move(placeholder));
}

void TagInput::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  if (mInput != nullptr) mInput->setFont(mFont);
  rebuild();
}

Size TagInput::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 260.0f, 34.0f });
  return measuredSize();
}

void TagInput::layout() {
  float left = bounds().getLeft();
  const float top = bounds().getTop() + 3.0f;
  const float height = std::max(0.0f, bounds().getHeight() - 6.0f);
  for (const auto& chip: mChips) {
    if (chip == nullptr) continue;
    const Size size = chip->measure({});
    chip->setBounds(Rect::MakeXYWH(left, top, size.width, height));
    chip->layout();
    left += size.width + mGap;
  }
  if (mInput != nullptr) {
    mInput->setBounds(Rect::MakeXYWH(left, top, std::max(40.0f, bounds().getRight() - left),
      height));
    mInput->layout();
  }
  clearNeedsLayout();
}

void TagInput::applyTheme(const Theme& theme) {
  mGap = theme.spacing.xs;
  setNeedsLayout();
}

void TagInput::rebuild() {
  for (const auto& chip: mChips) removeChild(chip);
  mChips.clear();
  const auto alive = mAlive;
  for (size_t index = 0; index < mTokens.size(); ++index) {
    auto chip = std::make_shared<Chip>(mTokens[index], mFont);
    chip->setRemovable(true);
    chip->setOnRemove([this, alive, index] {
      if (*alive) removeToken(static_cast<int>(index));
    });
    addChild(chip);
    mChips.push_back(std::move(chip));
  }
  if (mInput != nullptr) {
    if (mInput->parent() == this) removeChild(mInput);
    addChild(mInput);
  }
  setAccessibleValue(std::to_string(mTokens.size()));
  setNeedsLayout();
}

void TagInput::addToken(const std::string& token) {
  size_t begin = 0;
  size_t end = token.size();
  while (begin < end && (token[begin] == ' ' || token[begin] == '\t')) ++begin;
  while (end > begin && (token[end - 1] == ' ' || token[end - 1] == '\t')) --end;
  if (begin == end) return;
  mTokens.push_back(token.substr(begin, end - begin));
  if (mInput != nullptr) mInput->setText("");
  rebuild();
  if (mOnChanged) mOnChanged(mTokens);
}

void TagInput::removeToken(const int index) {
  if (index < 0 || index >= static_cast<int>(mTokens.size())) return;
  mTokens.erase(mTokens.begin() + index);
  rebuild();
  if (mOnChanged) mOnChanged(mTokens);
}

} // namespace coral::ui
