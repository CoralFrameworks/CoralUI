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

#include <ui/AvatarGroup.h>

#include <ui/Avatar.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

#include <algorithm>
#include <string>
#include <utility>

namespace coral::ui {

AvatarGroup::AvatarGroup(std::vector<std::string> initials,
    std::shared_ptr<const Font> font, const int maxVisible, const float size)
    : mInitials(std::move(initials)), mFont(std::move(font)),
      mMaxVisible(std::max(1, maxVisible)), mSize(std::max(16.0f, size)) {
  mStep = mSize * 0.68f;
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Avatars");
  setAccessibleValue(std::to_string(mInitials.size()));
  rebuild();
}

void AvatarGroup::setInitials(std::vector<std::string> initials) {
  mInitials = std::move(initials);
  setAccessibleValue(std::to_string(mInitials.size()));
  rebuild();
}

void AvatarGroup::setMaxVisible(const int count) {
  mMaxVisible = std::max(1, count);
  rebuild();
}

void AvatarGroup::setSize(const float size) {
  mSize = std::max(16.0f, size);
  mStep = mSize * 0.68f;
  rebuild();
}

Size AvatarGroup::measure(const Size& available) {
  (void)available;
  const int visible = std::min(mMaxVisible, static_cast<int>(mInitials.size()));
  const int extra = static_cast<int>(mInitials.size()) - visible;
  const float width = static_cast<float>(std::max(1, visible)) * mStep +
    (extra > 0 ? mSize + 4.0f : 0.0f);
  setMeasuredSize({ width, mSize });
  return measuredSize();
}

void AvatarGroup::layout() {
  const int visible = std::min(mMaxVisible, static_cast<int>(mInitials.size()));
  for (int index = 0; index < visible; ++index) {
    mAvatars[static_cast<size_t>(index)]->setBounds(Rect::MakeXYWH(
      bounds().getLeft() + static_cast<float>(index) * mStep, bounds().getTop(),
      mSize, mSize));
    mAvatars[static_cast<size_t>(index)]->layout();
  }
  if (mMore != nullptr) {
    mMore->setBounds(Rect::MakeXYWH(bounds().getLeft() + static_cast<float>(visible) * mStep + 2.0f,
      bounds().getTop(), mSize, mSize));
    mMore->layout();
  }
  clearNeedsLayout();
}

void AvatarGroup::applyTheme(const Theme& theme) {
  if (mMore != nullptr) mMore->setColor(theme.textMuted);
  setNeedsPaint();
}

void AvatarGroup::rebuild() {
  for (const auto& avatar: mAvatars) removeChild(avatar);
  mAvatars.clear();
  if (mMore != nullptr) {
    removeChild(mMore);
    mMore.reset();
  }
  const int visible = std::min(mMaxVisible, static_cast<int>(mInitials.size()));
  for (int index = 0; index < visible; ++index) {
    auto avatar = std::make_shared<Avatar>(mInitials[static_cast<size_t>(index)], mFont, mSize);
    addChild(avatar);
    mAvatars.push_back(std::move(avatar));
  }
  const int extra = static_cast<int>(mInitials.size()) - visible;
  if (extra > 0) {
    mMore = std::make_shared<TextLayer>("+" + std::to_string(extra), mFont);
    addChild(mMore);
  }
  setNeedsLayout();
  setNeedsPaint();
}

} // namespace coral::ui
