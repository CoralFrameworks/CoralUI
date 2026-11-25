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

#include <ui/Accessibility.h>
#include <ui/CheckBox.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

CheckBox::CheckBox(std::string label, std::shared_ptr<const graphics::Font> font,
    const bool checked)
    : mLabel(std::move(label)), mFont(std::move(font)), mChecked(checked) {
  setAccessibleRole(AccessibilityRole::CheckBox);
  setAccessibleSelected(checked);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ToggleChanged);
  setAccessibleName(mLabel);
}

void CheckBox::setLabel(std::string label) {
  mLabel = std::move(label);
  setNeedsLayout();
  setNeedsPaint();
}

void CheckBox::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void CheckBox::setColors(const graphics::Color background, const graphics::Color foreground,
    const graphics::Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

void CheckBox::setIndeterminate(const bool indeterminate) {
  if (mIndeterminate == indeterminate) return;
  mIndeterminate = indeterminate;
  if (mIndeterminate) {
    setAccessibleValue("Mixed");
    setAccessibleSelected(false);
  } else {
    setAccessibleValue({});
    setAccessibleSelected(mChecked);
  }
  setNeedsPaint();
}

void CheckBox::setChecked(const bool checked) {
  mIndeterminate = false;
  setAccessibleValue({});
  if (mChecked == checked) {
    return;
  }
  mChecked = checked;
  setAccessibleSelected(checked);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ToggleChanged);
  setNeedsPaint();
}

Size CheckBox::measure(const Size& available) {
  (void)available;
  const float textWidth = mFont != nullptr ? mFont->measureWidth(mLabel) : 80.0f;
  const float height = mFont != nullptr
    ? std::max(24.0f, mFont->metrics().ascent + mFont->metrics().descent + 8.0f)
    : 26.0f;
  setMeasuredSize({ 26.0f + textWidth + mGap, height });
  return measuredSize();
}

void CheckBox::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float boxSize = std::min(18.0f, bounds().getHeight() - 4.0f);
  const Rect box = Rect::MakeXYWH(bounds().getLeft() + 2.0f,
    bounds().getTop() + (bounds().getHeight() - boxSize) * 0.5f, boxSize, boxSize);

  // A named style replaces the theme copy for the box and the label; without one the control keeps
  // its own theme colours so existing pages do not change appearance.
  graphics::Color background = mPressed ? mPressedBackground : mBackground;
  graphics::Color borderColor = mChecked ? mAccent : mBorder;
  graphics::Color labelColor = mForeground;
  float radius = mRadius;
  float borderWidth = 1.5f;
  if (const auto styled = resolvedStyle()) {
    background = styled->background;
    borderColor = styled->border;
    labelColor = styled->foreground;
    radius = styled->radius;
    borderWidth = styled->borderWidth;
  }
  graphics::Paint backgroundPaint;
  backgroundPaint.color = background;
  backgroundPaint.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(box, radius), backgroundPaint);

  graphics::Paint borderPaint;
  borderPaint.color = borderColor;
  borderPaint.style = graphics::Paint::Style::kStroke;
  borderPaint.strokeWidth = borderWidth;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(box, radius), borderPaint);

  if (mIndeterminate) {
    graphics::Paint dash;
    dash.color = graphics::Color::White();
    dash.style = graphics::Paint::Style::kStroke;
    dash.strokeWidth = 2.0f;
    canvas.drawLine({ box.getLeft() + box.getWidth() * 0.25f,
        box.getTop() + box.getHeight() * 0.5f },
      { box.getRight() - box.getWidth() * 0.25f,
        box.getTop() + box.getHeight() * 0.5f }, dash);
  } else if (mChecked) {
    detail::drawCheckMark(canvas, box, graphics::Color::White());
  }

  if (mFont != nullptr) {
    detail::drawLabel(context, mLabel, bounds(), *mFont, labelColor, box.getRight() + mGap);
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool CheckBox::onPointerDown(const PointerEvent& event) {
  mPressed = bounds().contains(event.position);
  setNeedsPaint();
  return true;
}

bool CheckBox::onPointerUp(const PointerEvent& event) {
  const bool activated = mPressed && bounds().contains(event.position);
  mPressed = false;
  setNeedsPaint();
  if (activated) {
    activate();
  }
  return true;
}

bool CheckBox::onKeyDown(const KeyEvent& event) {
  if (!event.repeat && (event.key == Key::Space || event.key == Key::Enter)) {
    activate();
    return true;
  }
  return false;
}

void CheckBox::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void CheckBox::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mPressedBackground = theme.controls.backgroundPressed;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mGap = theme.spacing.sm;
  mRadius = theme.radius.sm;
  setNeedsLayout();
  setNeedsPaint();
}

void CheckBox::activate() {
  ++mActivations;
  if (mIndeterminate) {
    mIndeterminate = false;
    mChecked = true;
  } else {
    mChecked = !mChecked;
  }
  setAccessibleValue({});
  setAccessibleSelected(mChecked);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ToggleChanged);
  setNeedsPaint();
  if (mOnChanged) {
    mOnChanged(mChecked);
  }
}

bool CheckBox::performAccessibilityAction(const AccessibilityAction action, const std::string_view value) {
  (void)value;
  if (action != AccessibilityAction::Toggle) return false;
  if (mIndeterminate) {
    activate();
  } else {
    setChecked(!mChecked);
  }
  return true;
}

} // namespace coral::ui
