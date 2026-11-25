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

#include <ui/TimePicker.h>

#include "WidgetDrawing.h"

#include <ui/Button.h>
#include <ui/Stepper.h>
#include <ui/Theme.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

namespace coral::ui {

TimePicker::TimePicker(std::shared_ptr<const Font> font, const int hour, const int minute,
    const bool use24HourClock) : mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  mUse24HourClock = use24HourClock;
  mHour = std::clamp(hour, 0, 23);
  mMinute = std::clamp(minute, 0, 59);
  mHourStepper = std::make_shared<Stepper>(0.0, 23.0, mHour);
  mMinuteStepper = std::make_shared<Stepper>(0.0, 59.0, mMinute);
  mHourStepper->setOnChanged([this](const double value) {
    mHour = std::clamp(static_cast<int>(value), 0, 23);
    setNeedsPaint();
    notify();
  });
  mMinuteStepper->setOnChanged([this](const double value) {
    mMinute = std::clamp(static_cast<int>(value), 0, 59);
    setNeedsPaint();
    notify();
  });
  mDoneButton = std::make_shared<Button>("Done", mFont);
  mDoneButton->setOnClick([this] {
    if (mOnDone) mOnDone();
  });
  addChild(mHourStepper);
  addChild(mMinuteStepper);
  addChild(mDoneButton);
  setTime(mHour, mMinute);
}

void TimePicker::setTime(const int hour, const int minute) {
  int total = (hour * 60 + minute) % (24 * 60);
  if (total < 0) total += 24 * 60;
  mHour = total / 60;
  mMinute = total % 60;
  if (mHourStepper != nullptr) mHourStepper->setValue(mHour);
  if (mMinuteStepper != nullptr) mMinuteStepper->setValue(mMinute);
  setAccessibleValue(valueText());
  setNeedsPaint();
}

void TimePicker::setUse24HourClock(const bool use24Hour) {
  mUse24HourClock = use24Hour;
  setAccessibleValue(valueText());
  setNeedsPaint();
}

std::string TimePicker::valueText() const {
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

void TimePicker::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void TimePicker::setColors(const Color background, const Color foreground, const Color accent) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  setNeedsPaint();
}

Size TimePicker::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 260.0f, 144.0f });
  return measuredSize();
}

void TimePicker::layout() {
  const float left = bounds().getLeft();
  const float top = bounds().getTop();
  const float width = bounds().getWidth();
  const float cell = width * 0.5f;
  if (mHourStepper != nullptr) {
    mHourStepper->setBounds(Rect::MakeXYWH(left + 18.0f, top + 62.0f,
      std::max(0.0f, cell - 28.0f), 30.0f));
    mHourStepper->layout();
  }
  if (mMinuteStepper != nullptr) {
    mMinuteStepper->setBounds(Rect::MakeXYWH(left + cell + 10.0f, top + 62.0f,
      std::max(0.0f, cell - 28.0f), 30.0f));
    mMinuteStepper->layout();
  }
  if (mDoneButton != nullptr) {
    mDoneButton->setBounds(Rect::MakeXYWH(bounds().getRight() - 88.0f,
      bounds().getTop() + 102.0f, 72.0f, 30.0f));
    mDoneButton->layout();
  }
  clearNeedsLayout();
}

void TimePicker::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint background;
  background.color = mBackground;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), 4.0f), background);

  if (mFont == nullptr) return;
  const float cell = bounds().getWidth() * 0.5f;
  const float left = bounds().getLeft();
  const float top = bounds().getTop();
  detail::drawLabel(context, "Hour", Rect::MakeXYWH(left + 18.0f, top + 6.0f, cell - 28.0f, 22.0f),
    *mFont, mForeground, left + 18.0f);
  detail::drawLabel(context, "Minute",
    Rect::MakeXYWH(left + cell + 10.0f, top + 6.0f, cell - 28.0f, 22.0f),
    *mFont, mForeground, left + cell + 10.0f);

  char hourText[16]{};
  if (mUse24HourClock) {
    std::snprintf(hourText, sizeof(hourText), "%02d", mHour);
  } else {
    int hour = mHour % 12;
    if (hour == 0) hour = 12;
    std::snprintf(hourText, sizeof(hourText), "%d %s", hour, mHour < 12 ? "AM" : "PM");
  }
  char minuteText[8]{};
  std::snprintf(minuteText, sizeof(minuteText), "%02d", mMinute);
  detail::drawLabel(context, hourText, Rect::MakeXYWH(left + 18.0f, top + 30.0f, cell - 28.0f, 28.0f),
    *mFont, mAccent, left + 18.0f);
  detail::drawLabel(context, minuteText, Rect::MakeXYWH(left + cell + 10.0f, top + 30.0f,
    cell - 28.0f, 28.0f), *mFont, mAccent, left + cell + 10.0f);
}

bool TimePicker::onKeyDown(const KeyEvent& event) {
  switch (event.key) {
    case Key::Left:
      setTime(mHour, mMinute - 1);
      notify();
      return true;
    case Key::Right:
      setTime(mHour, mMinute + 1);
      notify();
      return true;
    case Key::Up:
      setTime(mHour + 1, mMinute);
      notify();
      return true;
    case Key::Down:
      setTime(mHour - 1, mMinute);
      notify();
      return true;
    case Key::PageUp:
      setTime(mHour, mMinute + 10);
      notify();
      return true;
    case Key::PageDown:
      setTime(mHour, mMinute - 10);
      notify();
      return true;
    case Key::Enter:
    case Key::Space:
      if (mOnDone) mOnDone();
      return true;
    case Key::Escape:
      if (mOnCancel) mOnCancel();
      return true;
    default:
      return false;
  }
}

void TimePicker::applyTheme(const Theme& theme) {
  mBackground = theme.surfaceRaised;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  setNeedsPaint();
}

void TimePicker::notify() {
  setAccessibleValue(valueText());
  if (mOnChanged) mOnChanged(mHour, mMinute);
}

} // namespace coral::ui
