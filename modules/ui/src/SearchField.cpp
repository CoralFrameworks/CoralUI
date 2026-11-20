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

#include <ui/SearchField.h>

#include "WidgetDrawing.h"

#include <ui/TextInput.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {
constexpr float kLeftIconWidth = 24.0f;
constexpr float kClearWidth = 26.0f;
} // namespace

SearchField::SearchField(std::string placeholder, std::shared_ptr<const graphics::Font> font)
    : mFont(std::move(font)) {
  mInput = std::make_shared<TextInput>("", mFont);
  mInput->setFocusable(false);
  mInput->setBordered(false);
  mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  mInput->setPlaceholder(std::move(placeholder));
  addChild(mInput);
}

SearchField::~SearchField() = default;

void SearchField::setText(std::string text) {
  if (mInput != nullptr) {
    mInput->setText(std::move(text));
  }
  setNeedsPaint();
}

const std::string& SearchField::text() const {
  static const std::string empty;
  return mInput != nullptr ? mInput->text() : empty;
}

void SearchField::setPlaceholder(std::string placeholder) {
  if (mInput != nullptr) {
    mInput->setPlaceholder(std::move(placeholder));
  }
  setNeedsPaint();
}

void SearchField::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  if (mInput != nullptr) {
    mInput->setFont(mFont);
  }
  setNeedsLayout();
  setNeedsPaint();
}

void SearchField::setColors(const graphics::Color background, const graphics::Color foreground,
    const graphics::Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  if (mInput != nullptr) {
    mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  }
  setNeedsPaint();
}

void SearchField::clear() {
  if (mInput == nullptr || mInput->text().empty()) {
    return;
  }
  mInput->setText({});
  notifyChanged();
  setNeedsPaint();
}

Size SearchField::measure(const Size& available) {
  (void)available;
  const float inputHeight = mInput != nullptr ? mInput->measure({}).height : 32.0f;
  setMeasuredSize({ 220.0f, inputHeight + 4.0f });
  return measuredSize();
}

void SearchField::layout() {
  if (mInput != nullptr) {
    mInput->setBounds(Rect::MakeXYWH(bounds().getLeft() + kLeftIconWidth,
      bounds().getTop() + 2.0f,
      std::max(0.0f, bounds().getWidth() - kLeftIconWidth - kClearWidth),
      std::max(0.0f, bounds().getHeight() - 4.0f)));
    mInput->layout();
  }
  clearNeedsLayout();
}

void SearchField::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint background;
  background.color = mBackground;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), background);

  graphics::Paint border;
  border.color = mFocused ? mFocus : mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);

  graphics::Paint icon;
  icon.color = mMuted;
  icon.style = graphics::Paint::Style::kStroke;
  icon.strokeWidth = 1.5f;
  const float centerX = bounds().getLeft() + 12.0f;
  const float centerY = bounds().getTop() + bounds().getHeight() * 0.5f;
  canvas.drawCircle(centerX, centerY, 5.0f, icon);
  canvas.drawLine({ centerX + 4.0f, centerY + 4.0f },
    { centerX + 8.0f, centerY + 8.0f }, icon);

  if (mInput != nullptr && !mInput->text().empty()) {
    graphics::Paint clearPaint;
    clearPaint.color = mForeground;
    clearPaint.style = graphics::Paint::Style::kStroke;
    clearPaint.strokeWidth = 1.5f;
    const float clearX = bounds().getRight() - 14.0f;
    canvas.drawLine({ clearX - 4.0f, centerY - 4.0f }, { clearX + 4.0f, centerY + 4.0f },
      clearPaint);
    canvas.drawLine({ clearX + 4.0f, centerY - 4.0f }, { clearX - 4.0f, centerY + 4.0f },
      clearPaint);
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool SearchField::onPointerDown(const PointerEvent& event) {
  if (event.position.x >= bounds().getRight() - kClearWidth) {
    clear();
    return true;
  }
  return true;
}

bool SearchField::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Enter) {
    if (mOnSearch) {
      mOnSearch(text());
    }
    return true;
  }
  if (event.key == Key::Escape) {
    clear();
    return true;
  }
  return mInput != nullptr ? mInput->onKeyDown(event) : false;
}

bool SearchField::onTextInput(const char32_t character) {
  const bool handled = mInput != nullptr && mInput->onTextInput(character);
  if (handled) {
    notifyChanged();
  }
  return handled;
}

bool SearchField::onTextComposition(const std::string& text) {
  return mInput != nullptr ? mInput->onTextComposition(text) : false;
}

bool SearchField::onTextCommit(const std::string& text) {
  const bool handled = mInput != nullptr && mInput->onTextCommit(text);
  if (handled) {
    notifyChanged();
  }
  return handled;
}

bool SearchField::onTextDelete(const int32_t index, const uint32_t length) {
  const bool handled = mInput != nullptr && mInput->onTextDelete(index, length);
  if (handled) {
    notifyChanged();
  }
  return handled;
}

void SearchField::onFocusChanged(const bool focused) {
  mFocused = focused;
  if (mInput != nullptr) {
    mInput->onFocusChanged(focused);
  }
  setNeedsPaint();
}

void SearchField::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mMuted = theme.controls.muted;
  mAccent = theme.controls.accent;
  mRadius = theme.radius.md;
  if (mInput != nullptr) {
    mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  }
  setNeedsLayout();
  setNeedsPaint();
}

void SearchField::notifyChanged() {
  if (mOnChanged) {
    mOnChanged(text());
  }
}

} // namespace coral::ui
