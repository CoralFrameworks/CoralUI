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

#include <ui/Banner.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Banner::Banner(std::string text, std::shared_ptr<const Font> font,
    const BannerSeverity severity) : mText(std::move(text)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleValue(mText);
  setSeverity(severity);
}

void Banner::setText(std::string text) {
  mText = std::move(text);
  setAccessibleValue(mText);
  setNeedsLayout();
  setNeedsPaint();
}

void Banner::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Banner::setSeverity(const BannerSeverity severity) {
  mSeverity = severity;
  switch (severity) {
    case BannerSeverity::Info:
      mBackground = Color::MakeRGBA8(40, 62, 92, 255);
      mForeground = Color::MakeRGBA8(226, 240, 255, 255);
      mAccent = Color::MakeRGBA8(120, 180, 255, 255);
      break;
    case BannerSeverity::Success:
      mBackground = Color::MakeRGBA8(38, 82, 63, 255);
      mForeground = Color::MakeRGBA8(230, 255, 242, 255);
      mAccent = Color::MakeRGBA8(110, 220, 165, 255);
      break;
    case BannerSeverity::Warning:
      mBackground = Color::MakeRGBA8(105, 79, 32, 255);
      mForeground = Color::MakeRGBA8(255, 247, 224, 255);
      mAccent = Color::MakeRGBA8(255, 205, 110, 255);
      break;
    case BannerSeverity::Danger:
      mBackground = Color::MakeRGBA8(108, 48, 54, 255);
      mForeground = Color::MakeRGBA8(255, 235, 237, 255);
      mAccent = Color::MakeRGBA8(255, 138, 148, 255);
      break;
  }
  setNeedsPaint();
}

void Banner::setAction(std::string label, std::function<void()> callback) {
  mActionLabel = std::move(label);
  mOnAction = std::move(callback);
  setNeedsLayout();
  setNeedsPaint();
}

void Banner::playEnterAnimation() {
  setOpacity(0.0f);
  setTranslation({ 0.0f, -6.0f });
  animateOpacity(1.0f, std::chrono::milliseconds(180), Easing::EaseOut);
  animateTranslation({ 0.0f, 0.0f }, std::chrono::milliseconds(180), Easing::EaseOut);
}

void Banner::playExitAnimation() {
  if (mExitPending) return;
  mExitPending = true;
  mExitStart = std::chrono::steady_clock::now();
  animateOpacity(0.0f, std::chrono::milliseconds(180), Easing::EaseIn);
  animateTranslation({ 0.0f, -6.0f }, std::chrono::milliseconds(180), Easing::EaseIn);
}

void Banner::setIcon(std::shared_ptr<const graphics::Image> icon) {
  mIcon = std::move(icon);
  setNeedsPaint();
}

void Banner::setDismissible(const bool dismissible) {
  mDismissible = dismissible;
  setNeedsLayout();
  setNeedsPaint();
}

Size Banner::measure(const Size& available) {
  (void)available;
  const float actionWidth = mActionLabel.empty() || mFont == nullptr
    ? 0.0f : mFont->measureWidth(mActionLabel) + 20.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + mPadding * 2.0f : 36.0f;
  setMeasuredSize({ 360.0f, std::max(34.0f, height) + 0.0f });
  (void)actionWidth;
  return measuredSize();
}

void Banner::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), fill);
  if (mFont == nullptr) return;

  float right = bounds().getRight() - mPadding;
  if (mDismissible) {
    graphics::Paint x;
    x.color = mForeground;
    x.style = graphics::Paint::Style::kStroke;
    x.strokeWidth = 1.5f;
    const float cx = right - 6.0f;
    const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
    canvas.drawLine({ cx - 3.0f, cy - 3.0f }, { cx + 3.0f, cy + 3.0f }, x);
    canvas.drawLine({ cx + 3.0f, cy - 3.0f }, { cx - 3.0f, cy + 3.0f }, x);
    right -= 22.0f;
  }
  if (!mActionLabel.empty()) {
    const float actionWidth = mFont->measureWidth(mActionLabel);
    detail::drawLabel(context, mActionLabel,
      Rect::MakeXYWH(right - actionWidth, bounds().getTop(), actionWidth, bounds().getHeight()),
      *mFont, mAccent, right - actionWidth);
    right -= actionWidth + 18.0f;
  }
  float contentLeft = bounds().getLeft() + mPadding;
  if (mIcon != nullptr && mIcon->width() > 0 && mIcon->height() > 0) {
    const float maxSide = 15.0f;
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
    contentLeft += 22.0f;
  }
  const Rect textBounds = Rect::MakeLTRB(contentLeft, bounds().getTop(),
    right, bounds().getBottom());
  detail::drawLabel(context, mText, textBounds, *mFont, mForeground, textBounds.getLeft());
}

void Banner::onFrame(const std::chrono::steady_clock::time_point now) {
  Layer::onFrame(now);
  if (!mExitPending) return;
  if (now - mExitStart < std::chrono::milliseconds(180)) return;
  mExitPending = false;
  setVisible(false);
  if (mOnDismiss) mOnDismiss();
}

bool Banner::onPointerDown(const PointerEvent& event) {
  if (!inputEnabled()) return true;
  if (mDismissible && event.position.x >= bounds().getRight() - 26.0f) {
    dismiss();
    return true;
  }
  if (!mActionLabel.empty() && mFont != nullptr) {
    const float actionWidth = mFont->measureWidth(mActionLabel) + 20.0f;
    if (event.position.x >= bounds().getRight() - mPadding - actionWidth) {
      if (mOnAction) mOnAction();
      return true;
    }
  }
  return true;
}

bool Banner::onKeyDown(const KeyEvent& event) {
  if (!inputEnabled()) return false;
  if (event.key == Key::Escape && mDismissible) {
    dismiss();
    return true;
  }
  return false;
}

bool Banner::performAccessibilityAction(const AccessibilityAction action, std::string_view) {
  if (action == AccessibilityAction::Cancel && mDismissible) {
    dismiss();
    return true;
  }
  if (action == AccessibilityAction::Invoke && mOnAction) {
    mOnAction();
    return true;
  }
  return false;
}

void Banner::applyTheme(const Theme& theme) {
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void Banner::dismiss() {
  setVisible(false);
  if (mOnDismiss) mOnDismiss();
}

} // namespace coral::ui
