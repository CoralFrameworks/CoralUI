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

#include <ui/Link.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <utility>

namespace coral::ui {

Link::Link(std::string text, std::shared_ptr<const Font> font, std::function<void()> onActivate)
    : mText(std::move(text)), mFont(std::move(font)), mOnActivate(std::move(onActivate)) {
  setAccessibleRole(AccessibilityRole::Link);
  setAccessibleName(mText);
}

void Link::setText(std::string text) {
  mText = std::move(text);
  setAccessibleName(mText);
  setNeedsLayout();
  setNeedsPaint();
}

void Link::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Link::setColors(const Color color, const Color hover, const Color focus) {
  mColor = color;
  mHoverColor = hover;
  mFocusColor = focus;
  setNeedsPaint();
}

Size Link::measure(const Size& available) {
  (void)available;
  const float width = mFont != nullptr ? mFont->measureWidth(mText) : 80.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 20.0f;
  setMeasuredSize({ width, height });
  return measuredSize();
}

void Link::drawSelf(RenderContext& context) {
  if (mFont == nullptr) return;
  const Color color = mFocused ? mFocusColor : (mHovered ? mHoverColor : mColor);
  detail::drawLabel(context, mText, bounds(), *mFont, color, bounds().getLeft());
  graphics::Paint underline;
  underline.color = color;
  underline.style = graphics::Paint::Style::kFill;
  context.canvas().drawRect(Rect::MakeXYWH(bounds().getLeft(),
    bounds().getBottom() - 1.0f, bounds().getWidth(), 1.0f), underline);
}

bool Link::onPointerDown(const PointerEvent&) {
  activate();
  return true;
}

bool Link::onPointerMove(const PointerEvent&) {
  if (!mHovered) {
    mHovered = true;
    setNeedsPaint();
  }
  return true;
}

bool Link::onPointerLeave() {
  if (mHovered) {
    mHovered = false;
    setNeedsPaint();
  }
  return true;
}

bool Link::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Enter || event.key == Key::Space) {
    activate();
    return true;
  }
  return false;
}

void Link::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

bool Link::performAccessibilityAction(const AccessibilityAction action, std::string_view) {
  if (action != AccessibilityAction::Invoke) return false;
  activate();
  return true;
}

void Link::activate() {
  if (mOnActivate) mOnActivate();
}

void Link::applyTheme(const Theme& theme) {
  mColor = theme.controls.accent;
  mHoverColor = theme.controls.borderFocused;
  mFocusColor = theme.controls.borderFocused;
  setNeedsPaint();
}

} // namespace coral::ui
