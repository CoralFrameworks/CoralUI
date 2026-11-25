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

#include <ui/Chip.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Chip::Chip(std::string text, std::shared_ptr<const Font> font)
    : mText(std::move(text)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Button);
  setAccessibleName(mText);
  setAccessibleSelected(false);
}

void Chip::setText(std::string text) {
  mText = std::move(text);
  setAccessibleName(mText);
  setNeedsLayout();
  setNeedsPaint();
}

void Chip::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Chip::setColors(const Color background, const Color foreground, const Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

void Chip::setVariant(const ChipVariant variant) {
  mVariant = variant;
  switch (variant) {
    case ChipVariant::Neutral:
      mBackground = Color::MakeRGBA8(52, 56, 66, 255);
      mForeground = Color::MakeRGBA8(226, 232, 240, 255);
      mAccent = Color::MakeRGBA8(86, 156, 255, 255);
      break;
    case ChipVariant::Accent:
      mBackground = Color::MakeRGBA8(40, 82, 145, 255);
      mForeground = Color::MakeRGBA8(235, 244, 255, 255);
      mAccent = Color::MakeRGBA8(120, 180, 255, 255);
      break;
    case ChipVariant::Success:
      mBackground = Color::MakeRGBA8(38, 104, 75, 255);
      mForeground = Color::MakeRGBA8(230, 255, 242, 255);
      mAccent = Color::MakeRGBA8(110, 220, 165, 255);
      break;
    case ChipVariant::Warning:
      mBackground = Color::MakeRGBA8(131, 94, 31, 255);
      mForeground = Color::MakeRGBA8(255, 247, 224, 255);
      mAccent = Color::MakeRGBA8(255, 205, 110, 255);
      break;
    case ChipVariant::Danger:
      mBackground = Color::MakeRGBA8(137, 53, 60, 255);
      mForeground = Color::MakeRGBA8(255, 235, 237, 255);
      mAccent = Color::MakeRGBA8(255, 138, 148, 255);
      break;
  }
  setNeedsPaint();
}

void Chip::setIcon(std::shared_ptr<const graphics::Image> icon) {
  mIcon = std::move(icon);
  setNeedsLayout();
  setNeedsPaint();
}

void Chip::setRemovable(const bool removable) {
  mRemovable = removable;
  setNeedsLayout();
  setNeedsPaint();
}

void Chip::setSelected(const bool selected) {
  mSelected = selected;
  setAccessibleSelected(mSelected);
  setNeedsPaint();
}

Size Chip::measure(const Size& available) {
  (void)available;
  const float textWidth = mFont != nullptr ? mFont->measureWidth(mText) : 24.0f;
  const float iconWidth = mIcon != nullptr ? 18.0f : 0.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 10.0f : 24.0f;
  setMeasuredSize({ textWidth + iconWidth + mPadding * 2.0f + (mRemovable ? 18.0f : 0.0f),
    std::max(24.0f, height) });
  return measuredSize();
}

void Chip::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float radius = bounds().getHeight() * 0.5f;
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), fill);
  if (mSelected) {
    graphics::Paint outline;
    outline.color = mAccent;
    outline.style = graphics::Paint::Style::kStroke;
    outline.strokeWidth = 1.5f;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), outline);
  }
  float contentLeft = bounds().getLeft() + mPadding;
  if (mIcon != nullptr && mIcon->width() > 0 && mIcon->height() > 0) {
    const float maxSide = 14.0f;
    const float scale = std::min(maxSide / static_cast<float>(mIcon->width()),
      maxSide / static_cast<float>(mIcon->height()));
    const float iconW = static_cast<float>(mIcon->width()) * scale;
    const float iconH = static_cast<float>(mIcon->height()) * scale;
    graphics::Paint iconPaint;
    iconPaint.color = mForeground;
    iconPaint.style = graphics::Paint::Style::kFill;
    canvas.save();
    canvas.translate(contentLeft, bounds().getTop() + (bounds().getHeight() - iconH) * 0.5f);
    canvas.scale(scale);
    canvas.drawImage(*mIcon, 0.0f, 0.0f, iconPaint);
    canvas.restore();
    contentLeft += 18.0f;
  }
  if (mFont != nullptr) {
    const Rect textBounds = Rect::MakeLTRB(contentLeft, bounds().getTop(),
      bounds().getRight() - (mRemovable ? 20.0f : mPadding), bounds().getBottom());
    detail::drawLabel(context, mText, textBounds, *mFont, mForeground, textBounds.getLeft());
  }
  if (mRemovable) {
    graphics::Paint x;
    x.color = mAccent;
    x.style = graphics::Paint::Style::kStroke;
    x.strokeWidth = 1.5f;
    const float cx = bounds().getRight() - 11.0f;
    const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
    canvas.drawLine({ cx - 3.0f, cy - 3.0f }, { cx + 3.0f, cy + 3.0f }, x);
    canvas.drawLine({ cx + 3.0f, cy - 3.0f }, { cx - 3.0f, cy + 3.0f }, x);
  }
}

bool Chip::onPointerDown(const PointerEvent& event) {
  if (!inputEnabled()) return true;
  if (mRemovable && event.position.x >= bounds().getRight() - 20.0f) remove();
  return true;
}

bool Chip::onKeyDown(const KeyEvent& event) {
  if (!inputEnabled()) return false;
  if (event.key == Key::Delete || event.key == Key::Backspace) {
    remove();
    return true;
  }
  return false;
}

void Chip::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mPadding = theme.spacing.sm;
  // Variant colours are semantic, not theme defaults: a chip added after the window theme was
  // installed must keep its Accent/Danger/etc. colours.
  if (mVariant != ChipVariant::Neutral) setVariant(mVariant);
  setNeedsLayout();
  setNeedsPaint();
}

void Chip::remove() {
  if (mOnRemove) mOnRemove();
}

} // namespace coral::ui
