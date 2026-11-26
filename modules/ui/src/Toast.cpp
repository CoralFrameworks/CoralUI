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

#include <ui/Toast.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Toast::Toast(std::string text, std::shared_ptr<const graphics::Font> font)
    : mText(std::move(text)), mFont(std::move(font)) {
  setVisible(false);
  setHitTestable(false);
  setZIndex(1100);
}

void Toast::setText(std::string text) {
  mText = std::move(text);
  setNeedsLayout();
  setNeedsPaint();
}

void Toast::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Toast::setColors(const graphics::Color background, const graphics::Color foreground) {
  mBackground = background;
  mForeground = foreground;
  setNeedsPaint();
}

void Toast::show(std::string text) {
  mText = std::move(text);
  mDeadline.reset();
  const Size size = measure({});
  if (bounds().isEmpty()) {
    place({ 24.0f, 24.0f });
  } else {
    setBounds(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), size.width, size.height));
    setVisible(true);
  }
  setNeedsPaint();
}

void Toast::showFor(std::string text, const std::chrono::milliseconds duration) {
  show(std::move(text));
  mDeadline = std::chrono::steady_clock::now() + duration;
}

void Toast::showAt(const Point& position) {
  place(position);
}

void Toast::showAt(const Rect& area) {
  const Size size = measure({});
  const Point position{
    area.getLeft() + (area.getWidth() - size.width) * 0.5f,
    area.getBottom() - size.height - 24.0f
  };
  showAt(position);
}

void Toast::hide() {
  mDeadline.reset();
  setVisible(false);
}

void Toast::onFrame(const std::chrono::steady_clock::time_point now) {
  Layer::onFrame(now);
  if (!visible() || !mDeadline.has_value()) {
    return;
  }
  if (now >= *mDeadline) {
    // The dismissal callback normally removes the OverlayHost entry, which can drop the last
    // external reference to this Toast while onFrame is still running. Keep it alive for the rest
    // of the callback, then let the entry release it.
    const auto keepAlive = weak_from_this().lock();
    hide();
    if (mOnDismissed) {
      mOnDismissed();
    }
  }
}

Size Toast::measure(const Size& available) {
  (void)available;
  const float width = (mFont != nullptr ? mFont->measureWidth(mText) : 100.0f) +
    mPadding * 2.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + mPadding + 2.0f
    : 28.0f;
  setMeasuredSize({ std::max(80.0f, width), std::max(24.0f, height) });
  return measuredSize();
}

void Toast::drawSelf(RenderContext& context) {
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

void Toast::applyTheme(const Theme& theme) {
  mBackground = theme.surfaceRaised;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mPadding = theme.spacing.md;
  mRadius = theme.radius.md;
  setNeedsLayout();
  setNeedsPaint();
}

void Toast::place(const Point& position) {
  const Size size = measure({});
  setBounds(Rect::MakeXYWH(position.x, position.y, size.width, size.height));
  setVisible(true);
  setNeedsPaint();
}

} // namespace coral::ui
