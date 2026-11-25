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
#include <ui/RadioButton.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

RadioButton::RadioButton(std::string label, std::shared_ptr<const graphics::Font> font,
    const bool selected)
    : mLabel(std::move(label)), mFont(std::move(font)), mSelected(selected) {
  setAccessibleRole(AccessibilityRole::Radio);
  setAccessibleName(mLabel);
  setAccessibleSelected(selected);
  notifyAccessibilityEvent(*this, AccessibilityEvent::Selected);
}

void RadioButton::setLabel(std::string label) {
  mLabel = std::move(label);
  setNeedsLayout();
  setNeedsPaint();
}

void RadioButton::setColors(const graphics::Color background, const graphics::Color foreground,
    const graphics::Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

void RadioButton::setSelected(const bool selected) {
  if (mSelected == selected) {
    return;
  }
  mSelected = selected;
  setAccessibleSelected(selected);
  notifyAccessibilityEvent(*this, AccessibilityEvent::Selected);
  setNeedsPaint();
}

Size RadioButton::measure(const Size& available) {
  (void)available;
  const float textWidth = mFont != nullptr ? mFont->measureWidth(mLabel) : 80.0f;
  const float height = mFont != nullptr
    ? std::max(24.0f, mFont->metrics().ascent + mFont->metrics().descent + 8.0f)
    : 26.0f;
  setMeasuredSize({ 26.0f + textWidth + mGap, height });
  return measuredSize();
}

void RadioButton::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const float size = std::min(18.0f, bounds().getHeight() - 4.0f);
  // Leave room for the indicator and its focus ring inside the row. Anchoring at left + 2 put the
  // focused ring one pixel outside bounds, where it was clipped into a flat edge.
  const float centerX = bounds().getLeft() + size * 0.5f + 4.0f;
  const float centerY = bounds().getTop() + bounds().getHeight() * 0.5f;
  const float radius = size * 0.5f;

  // A named style replaces the theme copy for the dot, ring and label; the selected inner dot stays
  // white so it remains visible on the accent colour.
  graphics::Color fill = mSelected ? mAccent : mBorder;
  graphics::Color labelColor = mForeground;
  float borderWidth = 1.5f;
  if (const auto styled = resolvedStyle()) {
    fill = mSelected ? styled->border : styled->background;
    labelColor = styled->foreground;
    borderWidth = styled->borderWidth;
  }
  // Draw the indicator with filled circles rather than a stroked path. A stroked circle was the
  // source of a stray horizontal rule under the selected row in the GPU path renderer.
  if (mFocused) {
    graphics::Paint focus;
    focus.color = mFocus;
    focus.style = graphics::Paint::Style::kFill;
    canvas.drawCircle(centerX, centerY, radius + 3.0f, focus);
  }

  graphics::Paint outer;
  outer.color = fill;
  outer.style = graphics::Paint::Style::kFill;
  canvas.drawCircle(centerX, centerY, radius, outer);

  if (!mSelected) {
    graphics::Paint inner;
    inner.color = mBackground;
    inner.style = graphics::Paint::Style::kFill;
    canvas.drawCircle(centerX, centerY, radius - borderWidth, inner);
  }

  if (mSelected) {
    graphics::Paint dot;
    dot.color = graphics::Color::White();
    dot.style = graphics::Paint::Style::kFill;
    canvas.drawCircle(centerX, centerY, radius * 0.42f, dot);
  }

  if (mFont != nullptr) {
    detail::drawLabel(context, mLabel, bounds(), *mFont, labelColor,
      bounds().getLeft() + size + mGap);
  }
}

bool RadioButton::onPointerDown(const PointerEvent& event) {
  mPressed = bounds().contains(event.position);
  setNeedsPaint();
  return true;
}

bool RadioButton::onPointerUp(const PointerEvent& event) {
  const bool activated = mPressed && bounds().contains(event.position);
  mPressed = false;
  setNeedsPaint();
  if (activated) {
    activate();
  }
  return true;
}

bool RadioButton::onKeyDown(const KeyEvent& event) {
  if (!event.repeat && (event.key == Key::Space || event.key == Key::Enter)) {
    activate();
    return true;
  }
  return false;
}

void RadioButton::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void RadioButton::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mGap = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsLayout();
  setNeedsPaint();
}

void RadioButton::activate() {
  if (mSelected) {
    return;
  }
  mSelected = true;
  setNeedsPaint();
  if (mOnSelected) {
    mOnSelected();
  }
}

bool RadioButton::performAccessibilityAction(const AccessibilityAction action, const std::string_view value) {
  (void)value;
  if (action != AccessibilityAction::Invoke && action != AccessibilityAction::Toggle) return false;
  setSelected(true);
  return true;
}

} // namespace coral::ui
