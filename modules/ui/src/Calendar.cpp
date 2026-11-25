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

#include <ui/Calendar.h>

#include "WidgetDrawing.h"

#include <ui/SystemSettings.h>
#include <ui/DateFormat.h>
#include <ui/Theme.h>

#include <array>
#include <cstdio>
#include <string>
#include <utility>

namespace coral::ui {

namespace {

constexpr float kHeaderHeight = 34.0f;
constexpr float kWeekdayHeight = 22.0f;
constexpr float kCellHeight = 26.0f;
constexpr float kPadding = 10.0f;

const std::array<const char*, 12> kMonthNames = {
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"
};
const std::array<const char*, 7> kWeekdayNames = {
  "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"
};

} // namespace

Calendar::Calendar(const std::chrono::year_month_day selected,
    std::shared_ptr<const Font> font) : mSelected(selected), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Calendar);
  const SystemLocale locale = currentSystemLocale();
  mMonthNames = locale.monthNames;
  mWeekdayNames = locale.weekdayNames;
  for (size_t index = 0; index < mMonthNames.size(); ++index) {
    if (mMonthNames[index].empty()) mMonthNames[index] = kMonthNames[index];
  }
  for (size_t index = 0; index < mWeekdayNames.size(); ++index) {
    if (mWeekdayNames[index].empty()) mWeekdayNames[index] = kWeekdayNames[index];
  }
  mFirstWeekday = std::clamp(locale.firstWeekday - 1, 0, 6);
  setDate(selected);
}

void Calendar::setDate(const std::chrono::year_month_day date) {
  if (!date.ok()) return;
  mSelected = date;
  mViewYear = static_cast<int>(date.year());
  mViewMonth = static_cast<unsigned>(date.month());
  setAccessibleValue(formatDate(mSelected, dateFormatIso()));
  setNeedsPaint();
}

void Calendar::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void Calendar::setFirstWeekday(const int firstWeekday) {
  mFirstWeekday = std::clamp(firstWeekday, 0, 6);
  setNeedsPaint();
}

void Calendar::setMonthNames(std::array<std::string, 12> names) {
  mMonthNames = std::move(names);
  setNeedsPaint();
}

void Calendar::setColors(const Color background, const Color foreground, const Color accent,
    const Color muted) {
  mBackground = background;
  mForeground = foreground;
  mAccent = accent;
  mMuted = muted;
  setNeedsPaint();
}

Size Calendar::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ 280.0f, kHeaderHeight + kWeekdayHeight + kCellHeight * 6.0f + kPadding * 2.0f });
  return measuredSize();
}

int Calendar::firstSlot() const {
  const std::chrono::year_month_day first{ std::chrono::year{ mViewYear },
    std::chrono::month{ mViewMonth }, std::chrono::day{ 1 } };
  const int weekday = static_cast<int>(
    std::chrono::weekday{ std::chrono::sys_days{ first } }.c_encoding());
  return (weekday - mFirstWeekday + 7) % 7;
}

int Calendar::daysInMonth() const {
  const std::chrono::year_month_day_last last{ std::chrono::year{ mViewYear },
    std::chrono::month_day_last{ std::chrono::month{ mViewMonth } } };
  return static_cast<int>(static_cast<unsigned>(last.day()));
}

void Calendar::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  const Rect b = bounds();
  const float left = b.getLeft() + kPadding;
  const float top = b.getTop() + kPadding;
  const float width = b.getWidth() - kPadding * 2.0f;

  if (mFont != nullptr) {
    const std::string header = mMonthNames[mViewMonth - 1] + " " +
      std::to_string(mViewYear);
    detail::drawLabel(context, header,
      Rect::MakeLTRB(left + 32.0f, top, b.getRight() - 32.0f, top + kHeaderHeight),
      *mFont, mForeground, left + 32.0f);
  }

  graphics::Paint arrow;
  arrow.color = mMuted;
  arrow.style = graphics::Paint::Style::kStroke;
  arrow.strokeWidth = 1.5f;
  const float centerY = top + kHeaderHeight * 0.5f;
  canvas.drawLine({ left + 10.0f, centerY }, { left + 18.0f, centerY - 5.0f }, arrow);
  canvas.drawLine({ left + 18.0f, centerY - 5.0f }, { left + 18.0f, centerY + 5.0f }, arrow);
  canvas.drawLine({ left + 18.0f, centerY + 5.0f }, { left + 10.0f, centerY }, arrow);
  canvas.drawLine({ b.getRight() - 10.0f, centerY }, { b.getRight() - 18.0f, centerY - 5.0f }, arrow);
  canvas.drawLine({ b.getRight() - 18.0f, centerY - 5.0f },
    { b.getRight() - 18.0f, centerY + 5.0f }, arrow);
  canvas.drawLine({ b.getRight() - 18.0f, centerY + 5.0f },
    { b.getRight() - 10.0f, centerY }, arrow);

  const float cellWidth = width / 7.0f;
  const float gridTop = top + kHeaderHeight + kWeekdayHeight;
  if (mFont != nullptr) {
    for (int col = 0; col < 7; ++col) {
      const Rect cell = Rect::MakeXYWH(left + cellWidth * col, top + kHeaderHeight,
        cellWidth, kWeekdayHeight);
      const std::string& weekday = mWeekdayNames[(col + mFirstWeekday) % 7];
      detail::drawLabel(context, weekday, cell, *mFont, mMuted,
        cell.getLeft() + (cellWidth - mFont->measureWidth(weekday)) * 0.5f);
    }
  }

  const int first = firstSlot();
  const int count = daysInMonth();
  const bool selectedInView = static_cast<int>(mSelected.year()) == mViewYear &&
    static_cast<unsigned>(mSelected.month()) == mViewMonth;
  for (int day = 1; day <= count; ++day) {
    const int slot = first + day - 1;
    const int row = slot / 7;
    const int col = slot % 7;
    const Rect cell = Rect::MakeXYWH(left + cellWidth * col, gridTop + kCellHeight * row,
      cellWidth, kCellHeight);
    if (selectedInView && static_cast<unsigned>(mSelected.day()) == static_cast<unsigned>(day)) {
      graphics::Paint fill;
      fill.color = mAccent;
      fill.style = graphics::Paint::Style::kFill;
      canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(
        Rect::MakeXYWH(cell.getLeft() + 2.0f, cell.getTop() + 1.0f,
          cell.getWidth() - 4.0f, cell.getHeight() - 2.0f), 4.0f), fill);
    }
    if (mFont != nullptr) {
      const std::string label = std::to_string(day);
      const float textWidth = mFont->measureWidth(label);
      detail::drawLabel(context, label, cell, *mFont,
        selectedInView && static_cast<unsigned>(mSelected.day()) == static_cast<unsigned>(day)
          ? mBackground : mForeground,
        cell.getLeft() + (cellWidth - textWidth) * 0.5f);
    }
  }
  detail::drawFocusRing(canvas, b, mFocused, mAccent, 4.0f, focusRingWidth());
}

bool Calendar::onPointerDown(const PointerEvent& event) {
  const Rect b = bounds();
  const float left = b.getLeft() + kPadding;
  const float top = b.getTop() + kPadding;
  const float width = b.getWidth() - kPadding * 2.0f;
  if (event.position.y >= top && event.position.y <= top + kHeaderHeight) {
    if (event.position.x <= left + 28.0f) {
      changeMonth(-1);
      return true;
    }
    if (event.position.x >= b.getRight() - 28.0f) {
      changeMonth(1);
      return true;
    }
    return true;
  }
  const float gridTop = top + kHeaderHeight + kWeekdayHeight;
  if (event.position.y < gridTop) return true;
  const float cellWidth = width / 7.0f;
  const int col = static_cast<int>((event.position.x - left) / cellWidth);
  const int row = static_cast<int>((event.position.y - gridTop) / kCellHeight);
  if (col < 0 || col >= 7 || row < 0 || row >= 6) return true;
  // `firstSlot()` is the grid slot occupied by day 1, not a day-number offset. Inverting the draw
  // mapping is `slot - firstSlot + 1`; adding `firstSlot` shifted every date after a month change.
  const int day = row * 7 + col - firstSlot() + 1;
  if (day >= 1 && day <= daysInMonth()) {
    selectDay(std::chrono::year_month_day{ std::chrono::year{ mViewYear },
      std::chrono::month{ mViewMonth }, std::chrono::day{ static_cast<unsigned>(day) } });
  }
  return true;
}

bool Calendar::onKeyDown(const KeyEvent& event) {
  std::chrono::sys_days day = std::chrono::sys_days{ mSelected };
  switch (event.key) {
    case Key::Left: day -= std::chrono::days{ 1 }; break;
    case Key::Right: day += std::chrono::days{ 1 }; break;
    case Key::Up: day -= std::chrono::days{ 7 }; break;
    case Key::Down: day += std::chrono::days{ 7 }; break;
    case Key::Enter:
    case Key::Space:
      selectDay(mSelected);
      return true;
    case Key::Escape:
      if (mOnCancel) mOnCancel();
      return true;
    default:
      return false;
  }
  setDate(std::chrono::year_month_day{ day });
  return true;
}

void Calendar::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

bool Calendar::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::SetValue) return false;
  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  if (std::sscanf(std::string(value).c_str(), "%d-%u-%u", &year, &month, &day) != 3) {
    return false;
  }
  const std::chrono::year_month_day date{ std::chrono::year{year},
    std::chrono::month{month}, std::chrono::day{day} };
  if (!date.ok()) return false;
  setDate(date);
  if (mOnSelect) mOnSelect(mSelected);
  return true;
}

void Calendar::applyTheme(const Theme& theme) {
  mBackground = theme.surfaceRaised;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mMuted = theme.controls.border;
  setNeedsPaint();
}

void Calendar::changeMonth(const int delta) {
  const std::chrono::year_month view = std::chrono::year_month{ std::chrono::year{ mViewYear },
    std::chrono::month{ mViewMonth } } + std::chrono::months{ delta };
  mViewYear = static_cast<int>(view.year());
  mViewMonth = static_cast<unsigned>(view.month());
  setNeedsPaint();
}

void Calendar::selectDay(const std::chrono::year_month_day date) {
  if (!date.ok()) return;
  mSelected = date;
  mViewYear = static_cast<int>(date.year());
  mViewMonth = static_cast<unsigned>(date.month());
  setAccessibleValue(formatDate(mSelected, dateFormatIso()));
  setNeedsPaint();
  if (mOnSelect) mOnSelect(mSelected);
}

} // namespace coral::ui
