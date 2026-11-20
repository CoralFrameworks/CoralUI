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

#include <ui/Tooltip.h>

#include "WidgetDrawing.h"

#include <ui/Positioner.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Tooltip::Tooltip(std::string text, std::shared_ptr<const graphics::Font> font)
    : mText(std::move(text)), mFont(std::move(font)) {
  setVisible(false);
  setHitTestable(false);
  setZIndex(1000);
}

void Tooltip::setText(std::string text) {
  mText = std::move(text);
  setNeedsLayout();
  setNeedsPaint();
}

void Tooltip::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Tooltip::setColors(const graphics::Color background, const graphics::Color foreground) {
  mBackground = background;
  mForeground = foreground;
  setNeedsPaint();
}

void Tooltip::showAt(const Point& position) {
  const Size size = measure({});
  setBounds(Rect::MakeXYWH(position.x + 12.0f, position.y + 14.0f, size.width, size.height));
  setVisible(true);
}

void Tooltip::showAt(const Rect& anchor) {
  const Size size = measure({});
  Rect available;
  if (parent() != nullptr) available = parent()->bounds();
  PlacementRequest request;
  request.anchor = anchor;
  request.size = size;
  request.available = available;
  request.preferred = Placement::BelowStart;
  request.gap = 6.0f;
  request.margin = Insets{ 4.0f, 4.0f, 4.0f, 4.0f };
  setBounds(placeFloating(request).bounds);
  setVisible(true);
}

void Tooltip::scheduleShowAt(const Point anchor, const std::chrono::milliseconds delay) {
  mPending = true;
  mPendingAnchor = anchor;
  mShowAt = std::chrono::steady_clock::now() + delay;
}

void Tooltip::cancelScheduled() {
  mPending = false;
}

void Tooltip::hide() {
  mPending = false;
  setVisible(false);
}

void Tooltip::onFrame(const std::chrono::steady_clock::time_point now) {
  if (!mPending || now < mShowAt) return;
  mPending = false;
  showAt(mPendingAnchor);
}

Size Tooltip::measure(const Size& available) {
  (void)available;
  const float width = (mFont != nullptr ? mFont->measureWidth(mText) : 80.0f) + mPadding * 2.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + mPadding + 2.0f
    : 24.0f;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void Tooltip::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint background;
  background.color = mBackground;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), background);

  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);

  if (mFont != nullptr) {
    detail::drawLabel(context, mText, bounds(), *mFont, mForeground, bounds().getLeft() + mPadding);
  }
}

void Tooltip::applyTheme(const Theme& theme) {
  mBackground = theme.surfaceRaised;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsLayout();
  setNeedsPaint();
}

} // namespace coral::ui
