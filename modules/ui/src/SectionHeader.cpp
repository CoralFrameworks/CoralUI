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

#include <ui/SectionHeader.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <utility>

namespace coral::ui {

SectionHeader::SectionHeader(std::string title, std::shared_ptr<const Font> font,
    std::string subtitle, std::shared_ptr<Layer> action)
    : mTitle(std::move(title)), mSubtitle(std::move(subtitle)), mFont(std::move(font)),
      mAction(std::move(action)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName(mTitle);
  if (mAction != nullptr) addChild(mAction);
}

void SectionHeader::setTitle(std::string title) {
  mTitle = std::move(title);
  setAccessibleName(mTitle);
  setNeedsLayout();
  setNeedsPaint();
}

void SectionHeader::setSubtitle(std::string subtitle) {
  mSubtitle = std::move(subtitle);
  setNeedsLayout();
  setNeedsPaint();
}

void SectionHeader::setAction(std::shared_ptr<Layer> action) {
  if (mAction != nullptr) removeChild(mAction);
  mAction = std::move(action);
  if (mAction != nullptr) addChild(mAction);
  setNeedsLayout();
  setNeedsPaint();
}

void SectionHeader::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

Size SectionHeader::measure(const Size& available) {
  (void)available;
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  const float height = line + (mSubtitle.empty() ? 0.0f : line + 2.0f);
  float width = mFont != nullptr ? mFont->measureWidth(mTitle) : 120.0f;
  if (mAction != nullptr) width += mAction->measure({}).width + mGap;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void SectionHeader::layout() {
  if (mAction != nullptr) {
    const Size size = mAction->measure({});
    mAction->setBounds(Rect::MakeXYWH(bounds().getRight() - size.width, bounds().getTop(),
      size.width, size.height));
    mAction->layout();
  }
  clearNeedsLayout();
}

void SectionHeader::drawSelf(RenderContext& context) {
  if (mFont == nullptr) return;
  const float line = mFont->metrics().ascent + mFont->metrics().descent;
  detail::drawLabel(context, mTitle,
    Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), bounds().getWidth() - (mAction != nullptr
      ? mAction->bounds().getWidth() + mGap : 0.0f), line),
    *mFont, mTitleColor, bounds().getLeft());
  if (!mSubtitle.empty()) {
    detail::drawLabel(context, mSubtitle,
      Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + line + 2.0f, bounds().getWidth(), line),
      *mFont, mSubtitleColor, bounds().getLeft());
  }
}

void SectionHeader::applyTheme(const Theme& theme) {
  mTitleColor = theme.controls.foreground;
  mSubtitleColor = theme.textMuted;
  setNeedsPaint();
}

} // namespace coral::ui
