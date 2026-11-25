/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ui/ColorButton.h>

#include <ui/ColorPicker.h>
#include <ui/Popup.h>
#include <ui/RenderContext.h>
#include <ui/Theme.h>

#include "WidgetDrawing.h"

#include <algorithm>
#include <utility>

namespace coral::ui {

ColorButton::ColorButton(Color color, std::string accessibleName) : mColor(color) {
  setAccessibleName(std::move(accessibleName));
  setAccessibleRole(AccessibilityRole::Button);
}

ColorButton::~ColorButton() = default;

void ColorButton::setColor(const Color color) {
  mColor = color;
  setNeedsPaint();
}

void ColorButton::setOnClick(std::function<void()> callback) {
  mOnClick = std::move(callback);
}

void ColorButton::setPickerEnabled(const bool enabled) {
  mPickerEnabled = enabled;
  if (!enabled || mPicker != nullptr) return;
  mPicker = std::make_shared<ColorPicker>();
  mPicker->setMode(ColorPickerMode::Hsv);
  mPicker->setColor(mColor);
  mPicker->setOnChanged([this](const Color color) {
    setColor(color);
    if (mOnPick != nullptr) mOnPick(color);
  });
  mPopup = std::make_shared<Popup>(mPicker);
}

void ColorButton::setOnPick(std::function<void(Color)> callback) {
  mOnPick = std::move(callback);
}

Size ColorButton::measure(const Size& available) {
  const float width = available.width > 0.0f ? std::min(available.width, mSize) : mSize;
  const float height = available.height > 0.0f ? std::min(available.height, mSize) : mSize;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void ColorButton::drawSelf(RenderContext& context) {
  Canvas& canvas = context.canvas();
  const Rect swatch = Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(),
    bounds().getWidth(), bounds().getHeight());
  if (mHovered || mPressed) {
    Paint hover;
    hover.color = mPressed ? Color::MakeRGBA8(0, 0, 0, 36) : Color::MakeRGBA8(255, 255, 255, 18);
    hover.style = Paint::Style::kFill;
    canvas.drawRoundRect(RoundRect::MakeRectRadius(swatch, mRadius), hover);
  }
  Paint fill;
  fill.color = mColor;
  fill.style = Paint::Style::kFill;
  canvas.drawRoundRect(RoundRect::MakeRectRadius(swatch, mRadius), fill);

  Paint border;
  border.color = mBorder;
  border.style = Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(RoundRect::MakeRectRadius(swatch, mRadius), border);
  detail::drawFocusRing(canvas, swatch, mFocused, mFocus, mRadius, focusRingWidth());
}

bool ColorButton::onPointerDown(const PointerEvent& event) {
  if (event.button != PointerButton::Primary) return false;
  mPressed = true;
  mHovered = true;
  setNeedsPaint();
  return true;
}

bool ColorButton::onPointerUp(const PointerEvent& event) {
  if (event.button != PointerButton::Primary) return false;
  const bool activateNow = mPressed && bounds().contains(event.position);
  mPressed = false;
  setNeedsPaint();
  if (activateNow) activate();
  return true;
}

bool ColorButton::onKeyDown(const KeyEvent& event) {
  if (event.key != Key::Enter && event.key != Key::Space) return false;
  activate();
  return true;
}

void ColorButton::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void ColorButton::applyTheme(const Theme& theme) {
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void ColorButton::activate() {
  ++mActivations;
  if (mPickerEnabled && mPopup != nullptr) {
    if (mPopup->isOpen()) {
      mPopup->hide();
    } else {
      Layer* root = this;
      while (root->parent() != nullptr) root = root->parent();
      if (root != this) {
        if (mPopup->parent() != root) root->addChild(mPopup);
        const Rect anchor = boundsInWindow();
        mPopup->showAt(Point{ anchor.getLeft(), anchor.getBottom() + 4.0f });
      }
    }
  }
  if (mOnClick != nullptr) mOnClick();
}

} // namespace coral::ui
