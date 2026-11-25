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

#include <ui/TimeField.h>

#include "WidgetDrawing.h"

#include <ui/TextInput.h>
#include <ui/Popup.h>
#include <ui/SystemSettings.h>
#include <ui/Theme.h>
#include <ui/TimePicker.h>
#include <ui/Window.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace coral::ui {

TimeField::TimeField(std::shared_ptr<const Font> font, const int hour, const int minute,
    const std::optional<bool> use24HourClock) : mFont(std::move(font)) {
  mUse24HourClock = use24HourClock.value_or(currentSystemLocale().uses24HourClock);
  mHour = std::clamp(hour, 0, 23);
  mMinute = std::clamp(minute, 0, 59);
  mInput = std::make_shared<TextInput>("", mFont);
  mInput->setFocusable(false);
  mInput->setBordered(false);
  mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  addChild(mInput);
  setAccessibleRole(AccessibilityRole::TextField);
  setTime(mHour, mMinute);
}

TimeField::~TimeField() {
  *mPickerAlive = false;
  if (mPickerPopup != nullptr) mPickerPopup->hide();
  mPickerPopup.reset();
  mTimePicker.reset();
}

void TimeField::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  if (mInput != nullptr) mInput->setFont(mFont);
  if (mTimePicker != nullptr) mTimePicker->setFont(mFont);
  setNeedsLayout();
  setNeedsPaint();
}

void TimeField::setColors(const Color background, const Color foreground, const Color accent) {
  mBackground = background;
  mForeground = foreground;
  mFocus = accent;
  if (mInput != nullptr) mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  setNeedsPaint();
}

void TimeField::setTime(const int hour, const int minute) {
  mHour = std::clamp(hour, 0, 23);
  mMinute = std::clamp(minute, 0, 59);
  if (mInput != nullptr) {
    mInput->setText(formatValue());
    setAccessibleValue(mInput->text());
  }
  if (mTimePicker != nullptr) mTimePicker->setTime(mHour, mMinute);
  setNeedsPaint();
}

void TimeField::setUse24HourClock(const bool use24Hour) {
  mUse24HourClock = use24Hour;
  if (mInput != nullptr) {
    mInput->setText(formatValue());
    setAccessibleValue(mInput->text());
  }
  if (mTimePicker != nullptr) mTimePicker->setUse24HourClock(mUse24HourClock);
  setNeedsPaint();
}

std::string TimeField::text() const {
  return mInput != nullptr ? mInput->text() : std::string();
}

Size TimeField::measure(const Size& available) {
  (void)available;
  const float inputHeight = mInput != nullptr ? mInput->measure({}).height : 32.0f;
  setMeasuredSize({ 174.0f, inputHeight + 4.0f });
  return measuredSize();
}

void TimeField::layout() {
  if (mInput != nullptr) {
    mInput->setBounds(Rect::MakeXYWH(bounds().getLeft() + mPadding, bounds().getTop() + mPadding,
      std::max(0.0f, bounds().getWidth() - mPadding * 2.0f - 24.0f),
      std::max(0.0f, bounds().getHeight() - mPadding * 2.0f)));
    mInput->layout();
  }
  clearNeedsLayout();
}

void TimeField::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), fill);
  graphics::Paint stroke;
  stroke.color = mFocused ? mFocus : mBorder;
  stroke.style = graphics::Paint::Style::kStroke;
  stroke.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), stroke);
  graphics::Paint arrow = stroke;
  arrow.color = inputEnabled() ? mForeground : mBorder;
  arrow.strokeWidth = 1.5f;
  const float cx = bounds().getRight() - 14.0f;
  const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
  canvas.drawLine({ cx - 4.0f, cy - 2.0f }, { cx, cy + 2.0f }, arrow);
  canvas.drawLine({ cx, cy + 2.0f }, { cx + 4.0f, cy - 2.0f }, arrow);
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool TimeField::onPointerDown(const PointerEvent& event) {
  if (!inputEnabled()) return true;
  if (event.position.x >= bounds().getRight() - 24.0f) showTimePicker();
  return true;
}

bool TimeField::onKeyDown(const KeyEvent& event) {
  switch (event.key) {
    case Key::Enter:
      commit();
      return true;
    case Key::Escape:
      if (mPickerPopup != nullptr && mPickerPopup->isOpen()) {
        mPickerPopup->hide();
        return true;
      }
      if (mInput != nullptr) mInput->setText(formatValue());
      setNeedsPaint();
      return true;
    case Key::Up:
      nudge(1);
      return true;
    case Key::Down:
      nudge(-1);
      return true;
    case Key::PageUp:
      nudge(60);
      return true;
    case Key::PageDown:
      nudge(-60);
      return true;
    default:
      return mInput != nullptr ? mInput->onKeyDown(event) : false;
  }
}

bool TimeField::onTextInput(const char32_t character) {
  return mInput != nullptr ? mInput->onTextInput(character) : false;
}

bool TimeField::onTextComposition(const std::string& text) {
  return mInput != nullptr ? mInput->onTextComposition(text) : false;
}

bool TimeField::onTextCommit(const std::string& text) {
  return mInput != nullptr ? mInput->onTextCommit(text) : false;
}

bool TimeField::onTextDelete(const int32_t index, const uint32_t length) {
  return mInput != nullptr ? mInput->onTextDelete(index, length) : false;
}

void TimeField::onFocusChanged(const bool focused) {
  mFocused = focused;
  if (mInput != nullptr) mInput->onFocusChanged(focused);
  if (!focused) commit();
  setNeedsPaint();
}

void TimeField::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mPadding = theme.spacing.xxs;
  mRadius = theme.radius.md;
  if (mInput != nullptr) {
    mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  }
  setNeedsLayout();
  setNeedsPaint();
}

std::string TimeField::formatValue() const {
  char buffer[16]{};
  if (mUse24HourClock) {
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", mHour, mMinute);
  } else {
    int hour = mHour % 12;
    if (hour == 0) hour = 12;
    std::snprintf(buffer, sizeof(buffer), "%d:%02d %s", hour, mMinute,
      mHour < 12 ? "AM" : "PM");
  }
  return buffer;
}

bool TimeField::parse(const std::string& text) {
  int hour = 0;
  int minute = 0;
  char suffix[3]{};
  const int count = std::sscanf(text.c_str(), "%d:%d%2s", &hour, &minute, suffix);
  if (count < 2 || minute < 0 || minute > 59) return false;
  std::string upper;
  for (char c: std::string(suffix)) upper.push_back(static_cast<char>(std::toupper(c)));
  if (upper == "AM" || upper == "PM") {
    if (hour < 1 || hour > 12) return false;
    const bool pm = upper == "PM";
    if (hour == 12) hour = 0;
    if (pm) hour += 12;
  }
  if (hour < 0 || hour > 23) return false;
  mHour = hour;
  mMinute = minute;
  return true;
}

void TimeField::commit() {
  if (mInput == nullptr) return;
  const std::string raw = mInput->text();
  const size_t begin = raw.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    mInput->setText(formatValue());
    return;
  }
  const size_t end = raw.find_last_not_of(" \t\r\n");
  const int previousHour = mHour;
  const int previousMinute = mMinute;
  if (!parse(raw.substr(begin, end - begin + 1))) {
    mInput->setText(formatValue());
    setNeedsPaint();
    return;
  }
  mInput->setText(formatValue());
  setAccessibleValue(mInput->text());
  if ((mHour != previousHour || mMinute != previousMinute) && mOnChanged) {
    mOnChanged(mHour, mMinute);
  }
  setNeedsPaint();
}

void TimeField::showTimePicker() {
  if (window() == nullptr || window()->root() == nullptr) return;
  if (mTimePicker == nullptr) {
    mTimePicker = std::make_shared<TimePicker>(mFont, mHour, mMinute, mUse24HourClock);
    mPickerPopup = std::make_shared<Popup>(mTimePicker);
    const std::weak_ptr<bool> alive = mPickerAlive;
    mTimePicker->setOnChanged([this, alive](const int hour, const int minute) {
      const auto guard = alive.lock();
      if (guard == nullptr || !*guard) return;
      const int previousHour = mHour;
      const int previousMinute = mMinute;
      mHour = hour;
      mMinute = minute;
      if (mInput != nullptr) {
        mInput->setText(formatValue());
        setAccessibleValue(mInput->text());
      }
      if ((mHour != previousHour || mMinute != previousMinute) && mOnChanged) {
        mOnChanged(mHour, mMinute);
      }
      setNeedsPaint();
    });
    mTimePicker->setOnDone([this, alive] {
      const auto guard = alive.lock();
      if (guard == nullptr || !*guard) return;
      if (mPickerPopup != nullptr) mPickerPopup->hide();
      if (window() != nullptr) window()->focusLayer(this);
    });
    mTimePicker->setOnCancel([this, alive] {
      const auto guard = alive.lock();
      if (guard == nullptr || !*guard) return;
      if (mPickerPopup != nullptr) mPickerPopup->hide();
      if (window() != nullptr) window()->focusLayer(this);
    });
  }
  mTimePicker->setTime(mHour, mMinute);
  if (mPickerPopup->parent() != window()->root().get()) {
    if (mPickerPopup->parent() != nullptr) {
      mPickerPopup->parent()->removeChild(mPickerPopup);
    }
    window()->root()->addChild(mPickerPopup);
  }
  mPickerPopup->showAt(boundsInWindow());
  if (window() != nullptr && mTimePicker != nullptr) window()->focusLayer(mTimePicker.get());
}

void TimeField::nudge(const int minutes) {
  int total = mHour * 60 + mMinute + minutes;
  total = (total % (24 * 60) + 24 * 60) % (24 * 60);
  const int previousHour = mHour;
  const int previousMinute = mMinute;
  mHour = total / 60;
  mMinute = total % 60;
  if (mInput != nullptr) {
    mInput->setText(formatValue());
    setAccessibleValue(mInput->text());
  }
  if ((mHour != previousHour || mMinute != previousMinute) && mOnChanged) {
    mOnChanged(mHour, mMinute);
  }
  setNeedsPaint();
}

bool TimeField::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::SetValue) return false;
  const std::string raw(value);
  if (!parse(raw)) return false;
  mInput->setText(formatValue());
  setAccessibleValue(mInput->text());
  if (mOnChanged) mOnChanged(mHour, mMinute);
  return true;
}

} // namespace coral::ui
