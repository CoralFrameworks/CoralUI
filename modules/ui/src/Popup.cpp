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

#include <ui/Popup.h>

#include <ui/Positioner.h>
#include <ui/Theme.h>

#include "WidgetDrawing.h"

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {
constexpr float kAnchorGap = 4.0f;
} // namespace

Popup::Popup(std::shared_ptr<Layer> content) {
  setFloating(true);
  setVisible(false);
  setZIndex(900);
  setContent(std::move(content));
}

void Popup::setContent(std::shared_ptr<Layer> content) {
  if (mContent != nullptr) {
    removeChild(mContent);
  }
  mContent = std::move(content);
  if (mContent != nullptr) {
    addChild(mContent);
  }
  setNeedsLayout();
  setNeedsPaint();
}

void Popup::setColors(const graphics::Color background, const graphics::Color border) {
  mBackground = background;
  mBorder = border;
  setNeedsPaint();
}

void Popup::showAt(const Point& position) {
  const Size size = measure({});
  setBounds(Rect::MakeXYWH(position.x, position.y, size.width, size.height));
  layout();
  setVisible(true);
}

void Popup::showAt(const Rect& anchor) {
  const Size size = measure({});
  Rect available;
  if (parent() != nullptr) available = parent()->bounds();
  PlacementRequest request;
  request.anchor = anchor;
  request.size = size;
  request.available = available;
  request.preferred = Placement::BelowStart;
  request.gap = kAnchorGap;
  request.margin = Insets{ 8.0f, 8.0f, 8.0f, 8.0f };
  setBounds(placeFloating(request).bounds);
  layout();
  setVisible(true);
}

void Popup::hide() {
  setVisible(false);
}

Size Popup::measure(const Size& available) {
  if (mContent == nullptr) {
    setMeasuredSize({ mPadding * 2.0f, mPadding * 2.0f });
    return measuredSize();
  }
  const Size inner = mContent->measure({
    std::max(0.0f, available.width - mPadding * 2.0f),
    std::max(0.0f, available.height - mPadding * 2.0f) });
  setMeasuredSize({ inner.width + mPadding * 2.0f, inner.height + mPadding * 2.0f });
  return measuredSize();
}

void Popup::layout() {
  if (mContent != nullptr) {
    mContent->setBounds(Rect::MakeXYWH(bounds().getLeft() + mPadding,
      bounds().getTop() + mPadding,
      std::max(0.0f, bounds().getWidth() - mPadding * 2.0f),
      std::max(0.0f, bounds().getHeight() - mPadding * 2.0f)));
    mContent->layout();
  }
  clearNeedsLayout();
}

void Popup::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  detail::drawShadow(canvas, bounds(), mRadius, mShadow);
  graphics::Paint background;
  background.color = mBackground;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), background);

  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);
}

void Popup::applyTheme(const Theme& theme) {
  mBackground = theme.surfaceRaised;
  mBorder = theme.controls.border;
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  mShadow = theme.shadow.md;
  setNeedsLayout();
  setNeedsPaint();
}

bool Popup::onPointerDown(const PointerEvent& event) {
  (void)event;
  return true;
}

} // namespace coral::ui
