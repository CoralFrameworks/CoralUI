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

#include <ui/ComboBox.h>

#include "WidgetDrawing.h"

#include <ui/Menu.h>
#include <ui/TextInput.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {
constexpr float kArrowWidth = 24.0f;
}

ComboBox::ComboBox(std::vector<std::string> options, std::shared_ptr<const Font> font,
    std::string text) : mOptions(std::move(options)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::ComboBox);
  mInput = std::make_shared<TextInput>(std::move(text), mFont);
  mInput->setFocusable(false);
  mInput->setBordered(false);
  mInput->setColors(Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  addChild(mInput);
  setAccessibleValue(mInput->text());
  rebuildMenu();
}

ComboBox::~ComboBox() {
  *mAlive = false;
  if (mMenu != nullptr) mMenu->hide();
  mMenu.reset();
}

void ComboBox::setOptions(std::vector<std::string> options) {
  mOptions = std::move(options);
  mSelected = std::clamp(mSelected, -1,
    mOptions.empty() ? -1 : static_cast<int>(mOptions.size()) - 1);
  rebuildMenu();
  setNeedsLayout();
  setNeedsPaint();
}

void ComboBox::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  if (mInput != nullptr) mInput->setFont(mFont);
  rebuildMenu();
  setNeedsLayout();
  setNeedsPaint();
}

void ComboBox::setColors(const Color background, const Color foreground, const Color accent) {
  mBackground = background;
  mForeground = foreground;
  mFocus = accent;
  if (mInput != nullptr) mInput->setColors(Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  setNeedsPaint();
}

void ComboBox::setPlaceholder(std::string placeholder) {
  if (mInput != nullptr) mInput->setPlaceholder(std::move(placeholder));
}

void ComboBox::setText(std::string text) {
  if (mInput != nullptr) {
    mInput->setText(std::move(text));
    setAccessibleValue(mInput->text());
  }
}

std::string ComboBox::text() const {
  return mInput != nullptr ? mInput->text() : std::string();
}

void ComboBox::setSelectedIndex(const int index) {
  mSelected = index >= 0 && index < static_cast<int>(mOptions.size()) ? index : -1;
  if (mSelected >= 0 && mInput != nullptr) {
    mInput->setText(mOptions[static_cast<size_t>(mSelected)]);
    setAccessibleValue(mInput->text());
  }
  setNeedsPaint();
}

Size ComboBox::measure(const Size& available) {
  (void)available;
  const float inputHeight = mInput != nullptr ? mInput->measure({}).height : 32.0f;
  setMeasuredSize({ 180.0f, inputHeight + 4.0f });
  return measuredSize();
}

void ComboBox::layout() {
  if (mInput != nullptr) {
    mInput->setBounds(Rect::MakeXYWH(bounds().getLeft() + mPadding, bounds().getTop() + 2.0f,
      std::max(0.0f, bounds().getWidth() - mPadding - kArrowWidth),
      std::max(0.0f, bounds().getHeight() - 4.0f)));
    mInput->layout();
  }
  clearNeedsLayout();
}

void ComboBox::drawSelf(RenderContext& context) {
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

  graphics::Paint arrow = stroke;
  arrow.color = inputEnabled() ? foreground : border;
  arrow.strokeWidth = 1.5f;
  const float cx = bounds().getRight() - kArrowWidth * 0.5f - 2.0f;
  const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
  canvas.drawLine({ cx - 4.0f, cy - 2.0f }, { cx, cy + 2.0f }, arrow);
  canvas.drawLine({ cx, cy + 2.0f }, { cx + 4.0f, cy - 2.0f }, arrow);
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, radius, focusRingWidth());
}

bool ComboBox::onPointerDown(const PointerEvent& event) {
  if (!inputEnabled()) return true;
  if (event.position.x >= bounds().getRight() - kArrowWidth) openMenu();
  return true;
}

bool ComboBox::onKeyDown(const KeyEvent& event) {
  if (!inputEnabled()) return false;
  switch (event.key) {
    case Key::Down:
      if (!mOptions.empty()) {
        choose(mSelected + 1 < static_cast<int>(mOptions.size()) ? mSelected + 1 : 0);
        return true;
      }
      break;
    case Key::Up:
      if (!mOptions.empty()) {
        choose(mSelected > 0 ? mSelected - 1 : static_cast<int>(mOptions.size()) - 1);
        return true;
      }
      break;
    case Key::Enter:
      if (mMenu != nullptr && mMenu->isOpen()) {
        mMenu->hide();
        return true;
      }
      break;
    case Key::Escape:
      if (mMenu != nullptr && mMenu->isOpen()) {
        mMenu->hide();
        return true;
      }
      break;
    default:
      break;
  }
  const bool handled = mInput != nullptr ? mInput->onKeyDown(event) : false;
  syncAccessibleText();
  return handled;
}

bool ComboBox::onTextInput(const char32_t character) {
  const bool handled = mInput != nullptr ? mInput->onTextInput(character) : false;
  syncAccessibleText();
  return handled;
}

bool ComboBox::onTextComposition(const std::string& text) {
  return mInput != nullptr ? mInput->onTextComposition(text) : false;
}

bool ComboBox::onTextCommit(const std::string& text) {
  const bool handled = mInput != nullptr ? mInput->onTextCommit(text) : false;
  syncAccessibleText();
  return handled;
}

bool ComboBox::onTextDelete(const int32_t index, const uint32_t length) {
  const bool handled = mInput != nullptr ? mInput->onTextDelete(index, length) : false;
  syncAccessibleText();
  return handled;
}

void ComboBox::onFocusChanged(const bool focused) {
  mFocused = focused;
  if (mInput != nullptr) mInput->onFocusChanged(focused);
  setNeedsPaint();
}

bool ComboBox::performAccessibilityAction(const AccessibilityAction action, std::string_view) {
  if (action != AccessibilityAction::Invoke && action != AccessibilityAction::Select) {
    return false;
  }
  openMenu();
  return true;
}

void ComboBox::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mPadding = theme.spacing.xxs;
  mRadius = theme.radius.md;
  if (mInput != nullptr) {
    mInput->setColors(Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  }
  setNeedsLayout();
  setNeedsPaint();
}

void ComboBox::rebuildMenu() {
  mMenu = std::make_shared<Menu>(mFont);
  const std::weak_ptr<bool> alive = mAlive;
  for (size_t index = 0; index < mOptions.size(); ++index) {
    mMenu->addItem(mOptions[index], [this, index, alive] {
      const auto guard = alive.lock();
      if (guard == nullptr || !*guard) return;
      choose(static_cast<int>(index));
      if (mMenu != nullptr) mMenu->hide();
    });
  }
}

void ComboBox::openMenu() {
  if (!inputEnabled() || mOptions.empty() || mMenu == nullptr || window() == nullptr ||
      window()->root() == nullptr) return;
  if (mMenu->parent() != window()->root().get()) {
    if (mMenu->parent() != nullptr) mMenu->parent()->removeChild(mMenu);
    window()->root()->addChild(mMenu);
  }
  mMenu->showAt(boundsInWindow());
}

void ComboBox::syncAccessibleText() {
  if (mInput != nullptr) setAccessibleValue(mInput->text());
}

void ComboBox::choose(const int index) {
  if (index < 0 || index >= static_cast<int>(mOptions.size())) return;
  mSelected = index;
  if (mInput != nullptr) {
    mInput->setText(mOptions[static_cast<size_t>(index)]);
    setAccessibleValue(mInput->text());
  }
  setNeedsPaint();
  if (mOnChanged) mOnChanged(mSelected, mOptions[static_cast<size_t>(index)]);
}

} // namespace coral::ui
