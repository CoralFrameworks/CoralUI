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

#include <ui/Expander.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui {

Expander::Expander(std::string title, std::shared_ptr<const Font> font,
    std::shared_ptr<Layer> content)
    : mTitle(std::move(title)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName(mTitle);
  setAccessibleValue("Expanded");
  setClipsToBounds(true);
  if (content != nullptr) setContent(std::move(content));
}

void Expander::setTitle(std::string title) {
  mTitle = std::move(title);
  setAccessibleName(mTitle);
  setNeedsPaint();
}

void Expander::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Expander::setHeaderHeight(const float height) {
  mHeaderHeight = std::max(0.0f, height);
  setNeedsLayout();
  setNeedsPaint();
}

void Expander::setContent(std::shared_ptr<Layer> content) {
  if (mContent != nullptr) removeChild(mContent);
  mContent = std::move(content);
  if (mContent != nullptr) {
    addChild(mContent);
    mContent->setVisible(mExpanded);
  }
  setNeedsLayout();
  setNeedsPaint();
}

void Expander::setColors(const Color headerBackground, const Color border,
    const Color foreground, const Color accent) {
  mHeaderBackground = headerBackground;
  mBorder = border;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

void Expander::setExpanded(const bool expanded) {
  if (mExpanded == expanded) return;
  mExpanded = expanded;
  setAccessibleValue(mExpanded ? "Expanded" : "Collapsed");
  if (mContent != nullptr) mContent->setVisible(mExpanded || mAnimating);
  if (mAnimated && mContent != nullptr) {
    mAnimationFrom = mExpansion;
    mAnimationTo = mExpanded ? 1.0f : 0.0f;
    mAnimationStart = std::chrono::steady_clock::now();
    mAnimating = true;
  } else {
    mExpansion = mExpanded ? 1.0f : 0.0f;
    if (mContent != nullptr) mContent->setVisible(mExpanded);
  }
  setNeedsLayout();
  setNeedsPaint();
  if (mOnChanged) mOnChanged(mExpanded);
}

Size Expander::measure(const Size& available) {
  float width = 180.0f;
  float contentHeight = 0.0f;
  if (mContent != nullptr && (mExpanded || mAnimating)) {
    const Size contentSize = mContent->measure({ available.width, available.height });
    width = std::max(width, contentSize.width);
    contentHeight = contentSize.height;
  }
  setMeasuredSize({ width, mHeaderHeight + contentHeight * mExpansion });
  return measuredSize();
}

void Expander::layout() {
  if (mContent != nullptr) {
    const float contentHeight = std::max(0.0f, bounds().getHeight() - mHeaderHeight) * mExpansion;
    mContent->setVisible(contentHeight > 0.0f);
    mContent->setBounds(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + mHeaderHeight,
      bounds().getWidth(), contentHeight));
    mContent->layout();
  }
  clearNeedsLayout();
}

void Expander::drawSelf(RenderContext& context) {
  const Rect header = Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(),
    bounds().getWidth(), mHeaderHeight);
  auto& canvas = context.canvas();
  graphics::Paint fill;
  fill.color = mHeaderBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(header, mRadius), fill);

  graphics::Paint stroke;
  stroke.color = mFocused ? mAccent : mBorder;
  stroke.style = graphics::Paint::Style::kStroke;
  stroke.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(header, mRadius), stroke);
  graphics::Paint divider;
  divider.color = mBorder;
  divider.style = graphics::Paint::Style::kFill;
  canvas.drawRect(Rect::MakeXYWH(header.getLeft() + mPadding,
    header.getBottom() - 1.0f, std::max(0.0f, header.getWidth() - mPadding * 2.0f), 1.0f),
    divider);

  if (mFont != nullptr) {
    const Rect textBounds = Rect::MakeLTRB(header.getLeft() + mPadding + 16.0f,
      header.getTop(), header.getRight() - mPadding, header.getBottom());
    detail::drawLabel(context, mTitle, textBounds, *mFont, mForeground, textBounds.getLeft());
  }

  graphics::Paint arrow = stroke;
  arrow.color = mForeground;
  arrow.strokeWidth = 1.5f;
  const float cx = header.getLeft() + mPadding + 2.0f;
  const float cy = header.getTop() + mHeaderHeight * 0.5f;
  // A right-facing chevron rotated by the expansion progress: collapsed points right, expanded down.
  const float angle = mExpansion * 1.5707963f;
  const float cosine = std::cos(angle);
  const float sine = std::sin(angle);
  const auto rotate = [cx, cy, cosine, sine](float x, float y) {
    return Point{ cx + x * cosine - y * sine, cy + x * sine + y * cosine };
  };
  const Point tip = rotate(2.0f, 0.0f);
  const Point top = rotate(-2.0f, -4.0f);
  const Point bottom = rotate(-2.0f, 4.0f);
  canvas.drawLine(top, tip, arrow);
  canvas.drawLine(tip, bottom, arrow);
  detail::drawFocusRing(canvas, header, mFocused, mAccent, mRadius, focusRingWidth());
}

bool Expander::onPointerDown(const PointerEvent& event) {
  if (event.position.y <= bounds().getTop() + mHeaderHeight) toggle();
  return true;
}

bool Expander::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Space || event.key == Key::Enter) {
    toggle();
    return true;
  }
  if (event.key == Key::Left) {
    setExpanded(false);
    return true;
  }
  if (event.key == Key::Right) {
    setExpanded(true);
    return true;
  }
  return false;
}

void Expander::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void Expander::onFrame(const std::chrono::steady_clock::time_point now) {
  Layer::onFrame(now);
  if (!mAnimating) return;
  constexpr auto duration = std::chrono::milliseconds(160);
  const float elapsed = std::chrono::duration<float>(now - mAnimationStart).count() /
    std::chrono::duration<float>(duration).count();
  const float t = std::clamp(elapsed, 0.0f, 1.0f);
  const float eased = 1.0f - (1.0f - t) * (1.0f - t);
  mExpansion = mAnimationFrom + (mAnimationTo - mAnimationFrom) * eased;
  if (t >= 1.0f) {
    mExpansion = mAnimationTo;
    mAnimating = false;
    if (mContent != nullptr) mContent->setVisible(mExpanded);
  }
  setNeedsLayout();
  setNeedsPaint();
}

bool Expander::performAccessibilityAction(const AccessibilityAction action, std::string_view) {
  if (action != AccessibilityAction::Toggle && action != AccessibilityAction::Expand) {
    return false;
  }
  toggle();
  return true;
}

void Expander::applyTheme(const Theme& theme) {
  mHeaderBackground = theme.surfaceRaised;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.borderFocused;
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void Expander::toggle() {
  setExpanded(!mExpanded);
}

} // namespace coral::ui
