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

#include <ui/FormField.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

FormField::FormField(std::string label, std::shared_ptr<Layer> content,
    std::shared_ptr<const Font> font)
    : mLabel(std::move(label)), mContent(std::move(content)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName(mLabel);
  if (mContent != nullptr) addChild(mContent);
}

void FormField::setLabel(std::string label) {
  mLabel = std::move(label);
  setAccessibleName(mLabel);
  setNeedsLayout();
  setNeedsPaint();
}

void FormField::setHelpText(std::string text) {
  mHelp = std::move(text);
  setNeedsLayout();
  setNeedsPaint();
}

void FormField::setErrorText(std::string text) {
  mError = std::move(text);
  setNeedsLayout();
  setNeedsPaint();
}

void FormField::setContent(std::shared_ptr<Layer> content) {
  if (mContent != nullptr) removeChild(mContent);
  mContent = std::move(content);
  if (mContent != nullptr) addChild(mContent);
  setNeedsLayout();
  setNeedsPaint();
}

void FormField::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

Size FormField::measure(const Size& available) {
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  const float note = (mHelp.empty() && mError.empty()) ? 0.0f : line + mGap;
  float width = mFont != nullptr ? mFont->measureWidth(mLabel) : 80.0f;
  if (mFont != nullptr) {
    width = std::max(width, mFont->measureWidth(mError.empty() ? mHelp : mError));
  }
  float contentHeight = 0.0f;
  if (mContent != nullptr) {
    const Size size = mContent->measure({ available.width, available.height });
    width = std::max(width, size.width);
    contentHeight = size.height;
  }
  setMeasuredSize({ width, line + mGap + contentHeight + note });
  return measuredSize();
}

void FormField::layout() {
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  float y = bounds().getTop();
  y += line + mGap;
  if (mContent != nullptr) {
    const Size size = mContent->measure({ bounds().getWidth(), 0.0f });
    mContent->setBounds(Rect::MakeXYWH(bounds().getLeft(), y, bounds().getWidth(), size.height));
    mContent->layout();
    y += size.height + mGap;
  }
  (void)line;
  (void)y;
  clearNeedsLayout();
}

void FormField::drawSelf(RenderContext& context) {
  if (mFont == nullptr) return;
  const float line = mFont->metrics().ascent + mFont->metrics().descent;
  detail::drawLabel(context, mLabel,
    Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), bounds().getWidth(), line),
    *mFont, mLabelColor, bounds().getLeft());
  if (!mError.empty() || !mHelp.empty()) {
    detail::drawLabel(context, mError.empty() ? mHelp : mError,
      Rect::MakeXYWH(bounds().getLeft(), bounds().getBottom() - line, bounds().getWidth(), line),
      *mFont, mError.empty() ? mHelpColor : mErrorColor, bounds().getLeft());
  }
}

void FormField::applyTheme(const Theme& theme) {
  mLabelColor = theme.controls.foreground;
  mHelpColor = theme.textMuted;
  mGap = theme.spacing.xs;
  setNeedsPaint();
}

} // namespace coral::ui
