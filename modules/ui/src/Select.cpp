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

#include <ui/Select.h>

#include "WidgetDrawing.h"

#include <ui/Menu.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Select::Select(std::vector<std::string> options, std::shared_ptr<const Font> font)
    : mOptions(std::move(options)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::ComboBox);
  rebuildMenu();
  setAccessibleValue(selectedText());
}

Select::~Select() {
  *mAlive = false;
  if (mMenu != nullptr) mMenu->hide();
  mMenu.reset();
}

void Select::setOptions(std::vector<std::string> options) {
  mOptions = std::move(options);
  mSelected = std::clamp(mSelected, 0,
    mOptions.empty() ? 0 : static_cast<int>(mOptions.size()) - 1);
  rebuildMenu();
  setAccessibleValue(selectedText());
  setNeedsLayout();
  setNeedsPaint();
}

void Select::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  rebuildMenu();
  setAccessibleValue(selectedText());
  setNeedsLayout();
  setNeedsPaint();
}

void Select::setColors(const Color background, const Color foreground, const Color accent) {
  mBackground = background;
  mForeground = foreground;
  mFocus = accent;
  setNeedsPaint();
}

void Select::setSelectedIndex(const int index) {
  if (mOptions.empty()) {
    mSelected = 0;
    return;
  }
  mSelected = std::clamp(index, 0, static_cast<int>(mOptions.size()) - 1);
  setAccessibleValue(selectedText());
  setNeedsPaint();
}

std::string Select::selectedText() const {
  return mSelected >= 0 && mSelected < static_cast<int>(mOptions.size())
    ? mOptions[static_cast<size_t>(mSelected)] : std::string();
}

Size Select::measure(const Size& available) {
  (void)available;
  float width = 120.0f;
  if (mFont != nullptr) {
    for (const std::string& option: mOptions) {
      width = std::max(width, mFont->measureWidth(option) + mPadding * 2.0f + 22.0f);
    }
  }
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 14.0f
    : 30.0f;
  setMeasuredSize({ width, std::max(28.0f, height) });
  return measuredSize();
}

void Select::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Color background = mBackground;
  graphics::Color border = mBorder;
  graphics::Color foreground = inputEnabled() ? mForeground : mMuted;
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

  if (mFont != nullptr) {
    const std::string value = selectedText().empty() ? mPlaceholder : selectedText();
    const Rect textBounds = Rect::MakeLTRB(bounds().getLeft() + mPadding,
      bounds().getTop(), bounds().getRight() - 24.0f, bounds().getBottom());
    detail::drawLabel(context, value, textBounds, *mFont,
      selectedText().empty() || !inputEnabled() ? mMuted : foreground,
      textBounds.getLeft());
  }

  graphics::Paint chevron = stroke;
  chevron.strokeWidth = 1.5f;
  const float cx = bounds().getRight() - 14.0f;
  const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
  canvas.drawLine({ cx - 4.0f, cy - 2.0f }, { cx, cy + 2.0f }, chevron);
  canvas.drawLine({ cx, cy + 2.0f }, { cx + 4.0f, cy - 2.0f }, chevron);
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, radius, focusRingWidth());
}

bool Select::onPointerDown(const PointerEvent&) {
  if (!inputEnabled()) return true;
  openMenu();
  return true;
}

bool Select::onKeyDown(const KeyEvent& event) {
  if (!inputEnabled()) return false;
  switch (event.key) {
    case Key::Space:
    case Key::Enter:
      openMenu();
      return true;
    case Key::Up:
      if (mOptions.empty()) return true;
      choose(mSelected > 0 ? mSelected - 1 : static_cast<int>(mOptions.size()) - 1);
      return true;
    case Key::Down:
      if (mOptions.empty()) return true;
      choose((mSelected + 1) % static_cast<int>(mOptions.size()));
      return true;
    default:
      return false;
  }
}

void Select::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

bool Select::performAccessibilityAction(const AccessibilityAction action, std::string_view) {
  if (action != AccessibilityAction::Invoke && action != AccessibilityAction::Select) {
    return false;
  }
  openMenu();
  return true;
}

void Select::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mMuted = theme.controls.border;
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsLayout();
  setNeedsPaint();
}

void Select::rebuildMenu() {
  mMenu = std::make_shared<Menu>(mFont);
  for (size_t index = 0; index < mOptions.size(); ++index) {
    mMenu->addItem(mOptions[index], [this, index] {
      choose(static_cast<int>(index));
      if (mMenu != nullptr) mMenu->hide();
    });
  }
}

void Select::openMenu() {
  if (!inputEnabled() || mOptions.empty() || mMenu == nullptr || window() == nullptr ||
      window()->root() == nullptr) return;
  if (mMenu->parent() != window()->root().get()) {
    if (mMenu->parent() != nullptr) mMenu->parent()->removeChild(mMenu);
    window()->root()->addChild(mMenu);
  }
  mMenu->showAt(boundsInWindow());
}

void Select::choose(const int index) {
  if (index < 0 || index >= static_cast<int>(mOptions.size())) return;
  const int previous = mSelected;
  mSelected = index;
  setAccessibleValue(selectedText());
  setNeedsPaint();
  if (mSelected != previous && mOnChanged) mOnChanged(mSelected);
}

} // namespace coral::ui
