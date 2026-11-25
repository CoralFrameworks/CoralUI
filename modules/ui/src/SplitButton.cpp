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

#include <ui/SplitButton.h>

#include "WidgetDrawing.h"

#include <ui/Menu.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {
constexpr float kArrowWidth = 28.0f;
}

SplitButton::SplitButton(std::string text, std::shared_ptr<const Font> font,
    std::vector<std::string> options)
    : mText(std::move(text)), mOptions(std::move(options)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Button);
  setAccessibleName(mText);
  mMenu = std::make_shared<Menu>(mFont);
  const std::weak_ptr<bool> alive = mAlive;
  for (size_t index = 0; index < mOptions.size(); ++index) {
    mMenu->addItem(mOptions[index], [this, index, alive] {
      const auto guard = alive.lock();
      if (guard == nullptr || !*guard) return;
      if (mMenu != nullptr) mMenu->hide();
      if (mOnOption) mOnOption(static_cast<int>(index), mOptions[index]);
    });
  }
}

SplitButton::~SplitButton() {
  *mAlive = false;
  if (mMenu != nullptr) mMenu->hide();
  mMenu.reset();
}

void SplitButton::setText(std::string text) {
  mText = std::move(text);
  setAccessibleName(mText);
  setNeedsLayout();
  setNeedsPaint();
}

void SplitButton::setOptions(std::vector<std::string> options) {
  mOptions = std::move(options);
  mMenu = std::make_shared<Menu>(mFont);
  const std::weak_ptr<bool> alive = mAlive;
  for (size_t index = 0; index < mOptions.size(); ++index) {
    mMenu->addItem(mOptions[index], [this, index, alive] {
      const auto guard = alive.lock();
      if (guard == nullptr || !*guard) return;
      if (mMenu != nullptr) mMenu->hide();
      if (mOnOption) mOnOption(static_cast<int>(index), mOptions[index]);
    });
  }
  setNeedsPaint();
}

void SplitButton::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setOptions(mOptions);
  setNeedsLayout();
  setNeedsPaint();
}

void SplitButton::setColors(const Color background, const Color foreground, const Color accent) {
  mBackground = background;
  mForeground = foreground;
  mFocus = accent;
  setNeedsPaint();
}

Size SplitButton::measure(const Size& available) {
  (void)available;
  const float textWidth = mFont != nullptr ? mFont->measureWidth(mText) : 60.0f;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 14.0f : 30.0f;
  setMeasuredSize({ std::max(96.0f, textWidth + mPadding * 2.0f + kArrowWidth),
    std::max(28.0f, height) });
  return measuredSize();
}

void SplitButton::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Color background = mBackground;
  graphics::Color border = mBorder;
  graphics::Color foreground = mForeground;
  float radius = mRadius;
  float borderWidth = 1.0f;
  if (const auto styled = resolvedStyle()) {
    background = styled->background;
    border = styled->border;
    foreground = styled->foreground;
    radius = styled->radius;
    borderWidth = styled->borderWidth;
  }
  graphics::Paint fill;
  fill.color = background;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), fill);
  graphics::Paint stroke;
  stroke.color = border;
  stroke.style = graphics::Paint::Style::kStroke;
  stroke.strokeWidth = borderWidth;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), radius), stroke);

  const float split = bounds().getRight() - kArrowWidth;
  canvas.drawLine({ split, bounds().getTop() + 4.0f },
    { split, bounds().getBottom() - 4.0f }, stroke);
  if (mFont != nullptr) {
    const Rect textBounds = Rect::MakeLTRB(bounds().getLeft() + mPadding, bounds().getTop(),
      split - 4.0f, bounds().getBottom());
    detail::drawLabel(context, mText, textBounds, *mFont, foreground, textBounds.getLeft());
  }
  graphics::Paint arrow = stroke;
  arrow.color = inputEnabled() ? foreground : border;
  arrow.strokeWidth = 1.5f;
  const float cx = split + kArrowWidth * 0.5f;
  const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
  canvas.drawLine({ cx - 4.0f, cy - 2.0f }, { cx, cy + 2.0f }, arrow);
  canvas.drawLine({ cx, cy + 2.0f }, { cx + 4.0f, cy - 2.0f }, arrow);
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, radius, focusRingWidth());
}

bool SplitButton::onPointerDown(const PointerEvent& event) {
  if (!inputEnabled()) return true;
  if (event.position.x >= bounds().getRight() - kArrowWidth) {
    openMenu();
  } else {
    activate();
  }
  return true;
}

bool SplitButton::onKeyDown(const KeyEvent& event) {
  if (!inputEnabled()) return false;
  if (event.key == Key::Enter || event.key == Key::Space) {
    activate();
    return true;
  }
  if (event.key == Key::Down && !mOptions.empty()) {
    openMenu();
    return true;
  }
  return false;
}

void SplitButton::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

bool SplitButton::performAccessibilityAction(const AccessibilityAction action, std::string_view) {
  if (action != AccessibilityAction::Invoke && action != AccessibilityAction::Confirm) {
    return false;
  }
  activate();
  return true;
}

void SplitButton::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void SplitButton::openMenu() {
  if (!inputEnabled() || mOptions.empty() || mMenu == nullptr || window() == nullptr ||
      window()->root() == nullptr) {
    return;
  }
  if (mMenu->parent() != window()->root().get()) {
    if (mMenu->parent() != nullptr) mMenu->parent()->removeChild(mMenu);
    window()->root()->addChild(mMenu);
  }
  mMenu->showAt(boundsInWindow());
}

void SplitButton::activate() {
  if (mOnClick) mOnClick();
}

} // namespace coral::ui
