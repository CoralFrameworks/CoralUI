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

#include <ui/NumberField.h>

#include "WidgetDrawing.h"

#include <ui/TextInput.h>
#include <ui/Theme.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <utility>

namespace coral::ui {

NumberField::NumberField(std::shared_ptr<const graphics::Font> font, const double minimum,
    const double maximum, const double value)
    : NumberField(std::move(font), minimum, maximum, value, false) {}

NumberField::NumberField(std::shared_ptr<const graphics::Font> font, const double minimum,
    const double maximum, const double value, const bool steppers)
    : mFont(std::move(font)), mSteppers(steppers) {
  mInput = std::make_shared<TextInput>("", mFont);
  mInput->setFocusable(false);
  mInput->setBordered(false);
  mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  addChild(mInput);
  setRange(minimum, maximum);
  setValue(value);
}

NumberField::~NumberField() = default;

void NumberField::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  if (mInput != nullptr) {
    mInput->setFont(mFont);
  }
  setNeedsLayout();
  setNeedsPaint();
}

void NumberField::setColors(const graphics::Color background, const graphics::Color foreground,
    const graphics::Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  if (mInput != nullptr) {
    mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  }
  setNeedsPaint();
}

void NumberField::setRange(const double minimum, const double maximum) {
  mMinimum = minimum;
  mMaximum = std::max(minimum + 1e-9, maximum);
  mValue = std::clamp(mValue, mMinimum, mMaximum);
  if (mInput != nullptr) {
    mInput->setText(formatValue());
  }
  setNeedsPaint();
}

void NumberField::setNumberFormat(const NumberFormatOptions& format) {
  mFormat = format;
  if (mInput != nullptr) mInput->setText(formatValue());
  setNeedsPaint();
}

void NumberField::setDecimals(const int decimals) {
  mDecimals = std::clamp(decimals, 0, 8);
  if (mInput != nullptr) {
    mInput->setText(formatValue());
  }
  setNeedsPaint();
}

void NumberField::setValue(const double value) {
  mValue = std::clamp(value, mMinimum, mMaximum);
  if (mInput != nullptr) {
    mInput->setText(formatValue());
  }
  setNeedsPaint();
}

Size NumberField::measure(const Size& available) {
  (void)available;
  const float inputHeight = mInput != nullptr ? mInput->measure({}).height : 32.0f;
  setMeasuredSize({ mSteppers ? 176.0f : 160.0f, inputHeight + 4.0f });
  return measuredSize();
}

void NumberField::layout() {
  if (mInput != nullptr) {
    const float rightInset = mSteppers ? 28.0f : mPadding;
    mInput->setBounds(Rect::MakeXYWH(bounds().getLeft() + mPadding,
      bounds().getTop() + mPadding,
      std::max(0.0f, bounds().getWidth() - mPadding - rightInset),
      std::max(0.0f, bounds().getHeight() - mPadding * 2.0f)));
    mInput->layout();
  }
  clearNeedsLayout();
}

void NumberField::drawSelf(RenderContext& context) {
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

  if (mSteppers) {
    const float dividerX = bounds().getRight() - 28.0f;
    graphics::Paint divider;
    divider.color = mBorder;
    divider.style = graphics::Paint::Style::kFill;
    canvas.drawRect(Rect::MakeXYWH(dividerX, bounds().getTop() + mPadding, 1.0f,
      bounds().getHeight() - mPadding * 2.0f), divider);

    graphics::Paint arrow;
    arrow.color = mForeground;
    arrow.style = graphics::Paint::Style::kStroke;
    arrow.strokeWidth = 1.5f;
    const float centerX = dividerX + 14.0f;
    const float upperY = bounds().getTop() + bounds().getHeight() * 0.30f;
    const float lowerY = bounds().getTop() + bounds().getHeight() * 0.70f;
    canvas.drawLine({ centerX - 4.0f, upperY + 2.0f }, { centerX, upperY - 2.0f }, arrow);
    canvas.drawLine({ centerX, upperY - 2.0f }, { centerX + 4.0f, upperY + 2.0f }, arrow);
    canvas.drawLine({ centerX - 4.0f, lowerY - 2.0f }, { centerX, lowerY + 2.0f }, arrow);
    canvas.drawLine({ centerX, lowerY + 2.0f }, { centerX + 4.0f, lowerY - 2.0f }, arrow);
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool NumberField::onPointerDown(const PointerEvent& event) {
  if (mSteppers && event.position.x >= bounds().getRight() - 28.0f) {
    const bool up = event.position.y < bounds().getTop() + bounds().getHeight() * 0.5f;
    nudge(up ? mStep : -mStep);
    return true;
  }
  return true;
}

bool NumberField::onKeyDown(const KeyEvent& event) {
  switch (event.key) {
    case Key::Up:
      nudge(mStep);
      return true;
    case Key::Down:
      nudge(-mStep);
      return true;
    case Key::Enter:
      commit();
      return true;
    case Key::Escape:
      if (mInput != nullptr) {
        mInput->setText(formatValue());
      }
      setNeedsPaint();
      return true;
    default:
      return mInput != nullptr ? mInput->onKeyDown(event) : false;
  }
}

bool NumberField::onTextInput(const char32_t character) {
  return mInput != nullptr ? mInput->onTextInput(character) : false;
}

bool NumberField::onTextComposition(const std::string& text) {
  return mInput != nullptr ? mInput->onTextComposition(text) : false;
}

bool NumberField::onTextCommit(const std::string& text) {
  return mInput != nullptr ? mInput->onTextCommit(text) : false;
}

bool NumberField::onTextDelete(const int32_t index, const uint32_t length) {
  return mInput != nullptr ? mInput->onTextDelete(index, length) : false;
}

void NumberField::onFocusChanged(const bool focused) {
  mFocused = focused;
  if (mInput != nullptr) {
    mInput->onFocusChanged(focused);
  }
  if (!focused) {
    commit();
  }
  setNeedsPaint();
}

void NumberField::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mPadding = theme.spacing.xxs;
  mRadius = theme.radius.md;
  if (mInput != nullptr) {
    mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  }
  setNeedsLayout();
  setNeedsPaint();
}

void NumberField::nudge(const double delta) {
  const double previous = mValue;
  setValue(mValue + delta);
  if (mValue != previous && mOnChanged) {
    mOnChanged(mValue);
  }
}

void NumberField::commit() {
  if (mInput == nullptr) {
    return;
  }
  const std::string& raw = mInput->text();
  const size_t begin = raw.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    mInput->setText(formatValue());
    return;
  }
  const size_t end = raw.find_last_not_of(" \t\r\n");
  const std::string text = raw.substr(begin, end - begin + 1);
  std::string normalized;
  normalized.reserve(text.size());
  for (size_t index = 0; index < text.size();) {
    if (!mFormat.groupSeparator.empty() &&
        text.compare(index, mFormat.groupSeparator.size(), mFormat.groupSeparator) == 0) {
      index += mFormat.groupSeparator.size();
      continue;
    }
    if (!mFormat.decimalSeparator.empty() &&
        text.compare(index, mFormat.decimalSeparator.size(), mFormat.decimalSeparator) == 0) {
      normalized.push_back('.');
      index += mFormat.decimalSeparator.size();
      continue;
    }
    normalized.push_back(text[index++]);
  }
  char* parsedEnd = nullptr;
  const double parsed = std::strtod(normalized.c_str(), &parsedEnd);
  if (parsedEnd == normalized.c_str() || *parsedEnd != '\0' || !std::isfinite(parsed)) {
    mInput->setText(formatValue());
    return;
  }
  const double previous = mValue;
  setValue(parsed);
  if (mValue != previous && mOnChanged) {
    mOnChanged(mValue);
  }
}

std::string NumberField::formatValue() const {
  NumberFormatOptions format = mFormat;
  format.maxFractionDigits = std::clamp(mDecimals, 0, 9);
  format.minFractionDigits = format.maxFractionDigits;
  return formatNumber(mValue, format);
}

} // namespace coral::ui
