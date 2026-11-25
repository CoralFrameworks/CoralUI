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

#include <ui/PropertyRow.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

PropertyRow::PropertyRow(std::string label, std::string value,
    std::shared_ptr<const Font> font)
    : mLabel(std::move(label)), mValue(std::move(value)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName(mLabel);
  setAccessibleValue(mValue);
}

void PropertyRow::setLabel(std::string label) {
  mLabel = std::move(label);
  setAccessibleName(mLabel);
  setNeedsPaint();
}

void PropertyRow::setValue(std::string value) {
  mValue = std::move(value);
  setAccessibleValue(mValue);
  setNeedsPaint();
}

void PropertyRow::setHelpText(std::string text) {
  mHelp = std::move(text);
  setNeedsLayout();
  setNeedsPaint();
}

void PropertyRow::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void PropertyRow::setLabelWidth(const float width) {
  mLabelWidth = std::max(40.0f, width);
  setNeedsLayout();
  setNeedsPaint();
}

Size PropertyRow::measure(const Size& available) {
  (void)available;
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  const float height = line + (mHelp.empty() ? 0.0f : line + 2.0f);
  setMeasuredSize({ mLabelWidth + mGap + 120.0f, height });
  return measuredSize();
}

void PropertyRow::drawSelf(RenderContext& context) {
  if (mFont == nullptr) return;
  const float line = mFont->metrics().ascent + mFont->metrics().descent;
  detail::drawLabel(context, mLabel,
    Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), mLabelWidth, line),
    *mFont, mLabelColor, bounds().getLeft());
  detail::drawLabel(context, mValue,
    Rect::MakeXYWH(bounds().getLeft() + mLabelWidth + mGap, bounds().getTop(),
      std::max(0.0f, bounds().getWidth() - mLabelWidth - mGap), line),
    *mFont, mValueColor, bounds().getLeft() + mLabelWidth + mGap);
  if (!mHelp.empty()) {
    detail::drawLabel(context, mHelp,
      Rect::MakeXYWH(bounds().getLeft() + mLabelWidth + mGap, bounds().getTop() + line + 2.0f,
        std::max(0.0f, bounds().getWidth() - mLabelWidth - mGap), line),
      *mFont, mHelpColor, bounds().getLeft() + mLabelWidth + mGap);
  }
}

void PropertyRow::applyTheme(const Theme& theme) {
  mLabelColor = theme.textMuted;
  mValueColor = theme.controls.foreground;
  setNeedsPaint();
}

} // namespace coral::ui
