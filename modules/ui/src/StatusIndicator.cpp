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

#include <ui/StatusIndicator.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <utility>

namespace coral::ui {

StatusIndicator::StatusIndicator(std::string label, std::shared_ptr<const Font> font,
    const StatusKind status)
    : mLabel(std::move(label)), mFont(std::move(font)), mStatus(status) {
  setAccessibleRole(AccessibilityRole::Label);
  setAccessibleName(mLabel);
  setAccessibleValue(mLabel);
}

void StatusIndicator::setLabel(std::string label) {
  mLabel = std::move(label);
  setAccessibleName(mLabel);
  setAccessibleValue(mLabel);
  setNeedsLayout();
  setNeedsPaint();
}

void StatusIndicator::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void StatusIndicator::setStatus(const StatusKind status) {
  mStatus = status;
  setNeedsPaint();
}

Size StatusIndicator::measure(const Size& available) {
  (void)available;
  const float textWidth = mFont != nullptr ? mFont->measureWidth(mLabel) : 40.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  setMeasuredSize({ mDot + mGap + textWidth, std::max(18.0f, height) });
  return measuredSize();
}

void StatusIndicator::drawSelf(RenderContext& context) {
  Color color = mOnline;
  switch (mStatus) {
    case StatusKind::Online: color = mOnline; break;
    case StatusKind::Away: color = mAway; break;
    case StatusKind::Busy: color = mBusy; break;
    case StatusKind::Offline: color = mOffline; break;
  }
  graphics::Paint dot;
  dot.color = color;
  dot.style = graphics::Paint::Style::kFill;
  const float centerY = bounds().getTop() + bounds().getHeight() * 0.5f;
  context.canvas().drawCircle(bounds().getLeft() + mDot * 0.5f, centerY, mDot * 0.5f, dot);
  if (mFont != nullptr) {
    detail::drawLabel(context, mLabel, bounds(), *mFont, mText,
      bounds().getLeft() + mDot + mGap);
  }
}

void StatusIndicator::applyTheme(const Theme& theme) {
  mText = theme.controls.foreground;
  setNeedsPaint();
}

} // namespace coral::ui
