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

#include <ui/Stat.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Stat::Stat(std::string label, std::string value, std::shared_ptr<const Font> font,
    std::string caption, const StatTrend trend)
    : mLabel(std::move(label)), mValue(std::move(value)), mCaption(std::move(caption)),
      mFont(std::move(font)), mTrend(trend) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName(mLabel);
  setAccessibleValue(mValue);
}

void Stat::setLabel(std::string label) {
  mLabel = std::move(label);
  setAccessibleName(mLabel);
  setNeedsPaint();
}

void Stat::setValue(std::string value) {
  mValue = std::move(value);
  setAccessibleValue(mValue);
  setNeedsPaint();
}

void Stat::setCaption(std::string caption) {
  mCaption = std::move(caption);
  setNeedsLayout();
  setNeedsPaint();
}

void Stat::setTrend(const StatTrend trend) {
  mTrend = trend;
  setNeedsPaint();
}

void Stat::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

Size Stat::measure(const Size& available) {
  (void)available;
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  const float height = line * 2.0f + (mCaption.empty() ? 0.0f : line) + mGap * 2.0f;
  float width = mFont != nullptr
    ? std::max(mFont->measureWidth(mLabel), mFont->measureWidth(mValue)) : 100.0f;
  if (mFont != nullptr && !mCaption.empty()) {
    width = std::max(width, mFont->measureWidth(mCaption));
  }
  setMeasuredSize({ width, height });
  return measuredSize();
}

void Stat::drawSelf(RenderContext& context) {
  if (mFont == nullptr) return;
  const float line = mFont->metrics().ascent + mFont->metrics().descent;
  float top = bounds().getTop();
  detail::drawLabel(context, mLabel,
    Rect::MakeXYWH(bounds().getLeft(), top, bounds().getWidth(), line), *mFont, mLabelColor,
    bounds().getLeft());
  top += line + mGap;
  detail::drawLabel(context, mValue,
    Rect::MakeXYWH(bounds().getLeft(), top, bounds().getWidth(), line), *mFont, mValueColor,
    bounds().getLeft());
  top += line + mGap;
  if (!mCaption.empty()) {
    const std::string arrow = mTrend == StatTrend::Up ? "\xE2\x86\x91 "
      : (mTrend == StatTrend::Down ? "\xE2\x86\x93 " : "");
    const Color color = mTrend == StatTrend::Up ? mUpColor
      : (mTrend == StatTrend::Down ? mDownColor : mFlatColor);
    detail::drawLabel(context, arrow + mCaption,
      Rect::MakeXYWH(bounds().getLeft(), top, bounds().getWidth(), line), *mFont, color,
      bounds().getLeft());
  }
}

void Stat::applyTheme(const Theme& theme) {
  mLabelColor = theme.textMuted;
  mValueColor = theme.controls.foreground;
  setNeedsPaint();
}

} // namespace coral::ui
