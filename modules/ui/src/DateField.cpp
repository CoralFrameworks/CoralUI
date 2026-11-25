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

#include <ui/DateField.h>
#include <ui/Calendar.h>
#include <ui/DatePickerPanel.h>
#include <ui/Popup.h>
#include <ui/Window.h>

#include "WidgetDrawing.h"

#include <ui/TextInput.h>
#include <ui/Theme.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>
#include <vector>

namespace coral::ui {

DateField::DateField(std::shared_ptr<const Font> font, const std::chrono::year_month_day date)
    : mFont(std::move(font)), mDate(date) {
  if (!mDate.ok()) {
    const std::chrono::year_month_day today{
      std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) };
    mDate = today;
  }
  mInput = std::make_shared<TextInput>("", mFont);
  mInput->setFocusable(false);
  mInput->setBordered(false);
  mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  addChild(mInput);
  setAccessibleRole(AccessibilityRole::TextField);
  setDate(mDate);
}

DateField::~DateField() {
  if (mCalendarAlive != nullptr) *mCalendarAlive = false;
  if (mCalendarPopup != nullptr) mCalendarPopup->hide();
  mCalendarPopup.reset();
  mCalendar.reset();
  if (supportsNativeDatePicker() && isNativeDatePickerVisible()) hideNativeDatePicker();
}

void DateField::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  if (mInput != nullptr) mInput->setFont(mFont);
  if (mCalendar != nullptr) mCalendar->setFont(mFont);
  setNeedsLayout();
  setNeedsPaint();
}

void DateField::setColors(const Color background, const Color foreground, const Color accent) {
  mBackground = background;
  mForeground = foreground;
  mFocus = accent;
  if (mInput != nullptr) {
    mInput->setColors(graphics::Color::MakeRGBA8(0, 0, 0, 0), mForeground);
  }
  setNeedsPaint();
}

void DateField::setFormat(const DateFormatOptions& format) {
  mFormat = format;
  if (mInput != nullptr) mInput->setText(formatValue());
  setNeedsPaint();
}

void DateField::setDate(const std::chrono::year_month_day date) {
  if (date.ok()) mDate = date;
  if (mInput != nullptr) mInput->setText(formatValue());
  if (mCalendar != nullptr) mCalendar->setDate(mDate);
  setNeedsPaint();
}

std::string DateField::text() const {
  return mInput != nullptr ? mInput->text() : std::string();
}

Size DateField::measure(const Size& available) {
  (void)available;
  const float inputHeight = mInput != nullptr ? mInput->measure({}).height : 32.0f;
  setMeasuredSize({ 180.0f, inputHeight + 4.0f });
  return measuredSize();
}

void DateField::layout() {
  if (mInput != nullptr) {
    mInput->setBounds(Rect::MakeXYWH(bounds().getLeft() + mPadding, bounds().getTop() + mPadding,
      std::max(0.0f, bounds().getWidth() - mPadding * 2.0f),
      std::max(0.0f, bounds().getHeight() - mPadding * 2.0f)));
    mInput->layout();
  }
  clearNeedsLayout();
}

void DateField::drawSelf(RenderContext& context) {
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
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool DateField::onPointerDown(const PointerEvent&) {
  if (mNativePickerEnabled && supportsNativeDatePicker()) {
    showNativeDatePicker(mDate, [this](const std::chrono::year_month_day date) {
      const auto previous = mDate;
      setDate(date);
      if (mDate != previous && mOnChanged) mOnChanged(mDate);
    });
  } else {
    showCalendar();
  }
  return true;
}

void DateField::showCalendar() {
  if (window() == nullptr || window()->root() == nullptr) return;
  if (mCalendar == nullptr) {
    mCalendar = std::make_shared<Calendar>(mDate, mFont);
    mCalendarPopup = std::make_shared<Popup>(mCalendar);
    const std::weak_ptr<bool> alive = mCalendarAlive;
    mCalendar->setOnSelect([this, alive](const std::chrono::year_month_day date) {
      const auto guard = alive.lock();
      if (guard == nullptr || !*guard) return;
      const auto previous = mDate;
      setDate(date);
      if (mDate != previous && mOnChanged) mOnChanged(mDate);
      if (mCalendarPopup != nullptr) mCalendarPopup->hide();
      if (window() != nullptr) window()->focusLayer(this);
    });
    mCalendar->setOnCancel([this, alive] {
      const auto guard = alive.lock();
      if (guard == nullptr || !*guard) return;
      if (mCalendarPopup != nullptr) mCalendarPopup->hide();
      if (window() != nullptr) window()->focusLayer(this);
    });
  }
  mCalendar->setDate(mDate);
  if (mCalendarPopup->parent() != window()->root().get()) {
    if (mCalendarPopup->parent() != nullptr) {
      mCalendarPopup->parent()->removeChild(mCalendarPopup);
    }
    window()->root()->addChild(mCalendarPopup);
  }
  mCalendarPopup->showAt(boundsInWindow());
  if (window() != nullptr && mCalendar != nullptr) window()->focusLayer(mCalendar.get());
}

bool DateField::onKeyDown(const KeyEvent& event) {
  switch (event.key) {
    case Key::Up: nudge(1); return true;
    case Key::Down: nudge(-1); return true;
    case Key::Enter: commit(); return true;
    case Key::Escape:
      if (mCalendarPopup != nullptr && mCalendarPopup->isOpen()) {
        mCalendarPopup->hide();
        return true;
      }
      if (mInput != nullptr) mInput->setText(formatValue());
      setNeedsPaint();
      return true;
    default:
      return mInput != nullptr ? mInput->onKeyDown(event) : false;
  }
}

bool DateField::onTextInput(const char32_t character) {
  return mInput != nullptr ? mInput->onTextInput(character) : false;
}

bool DateField::onTextComposition(const std::string& text) {
  return mInput != nullptr ? mInput->onTextComposition(text) : false;
}

bool DateField::onTextCommit(const std::string& text) {
  return mInput != nullptr ? mInput->onTextCommit(text) : false;
}

bool DateField::onTextDelete(const int32_t index, const uint32_t length) {
  return mInput != nullptr ? mInput->onTextDelete(index, length) : false;
}

void DateField::onFocusChanged(const bool focused) {
  mFocused = focused;
  if (mInput != nullptr) mInput->onFocusChanged(focused);
  if (!focused) commit();
  setNeedsPaint();
}

void DateField::applyTheme(const Theme& theme) {
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

std::string DateField::formatValue() const {
  return formatDate(mDate, mFormat);
}

bool DateField::parse(const std::string& text) {
  std::vector<std::string> parts;
  size_t begin = 0;
  while (begin <= text.size()) {
    const size_t end = text.find(mFormat.separator, begin);
    parts.push_back(text.substr(begin,
      end == std::string::npos ? std::string::npos : end - begin));
    if (end == std::string::npos) break;
    begin = end + mFormat.separator.size();
  }
  if (parts.size() != 3) return false;
  int values[3]{};
  for (size_t index = 0; index < 3; ++index) {
    const std::string& part = parts[index];
    if (part.empty() || !std::all_of(part.begin(), part.end(),
          [](const unsigned char c) { return std::isdigit(c) != 0; })) {
      return false;
    }
    values[index] = std::stoi(part);
  }
  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  switch (mFormat.order) {
    case DateOrder::YearMonthDay:
      year = values[0]; month = static_cast<unsigned>(values[1]);
      day = static_cast<unsigned>(values[2]); break;
    case DateOrder::DayMonthYear:
      day = static_cast<unsigned>(values[0]); month = static_cast<unsigned>(values[1]);
      year = values[2]; break;
    case DateOrder::MonthDayYear:
      month = static_cast<unsigned>(values[0]); day = static_cast<unsigned>(values[1]);
      year = values[2]; break;
  }
  const std::chrono::year_month_day candidate{ std::chrono::year{year},
    std::chrono::month{month}, std::chrono::day{day} };
  if (!candidate.ok()) return false;
  mDate = candidate;
  return true;
}

void DateField::commit() {
  if (mInput == nullptr) return;
  const std::string raw = mInput->text();
  const size_t begin = raw.find_first_not_of(" \\t\\r\\n");
  if (begin == std::string::npos) {
    mInput->setText(formatValue());
    return;
  }
  const size_t end = raw.find_last_not_of(" \\t\\r\\n");
  const std::string text = raw.substr(begin, end - begin + 1);
  const auto previous = mDate;
  if (!parse(text)) {
    mInput->setText(formatValue());
    setNeedsPaint();
    return;
  }
  mInput->setText(formatValue());
  if (mDate != previous && mOnChanged) mOnChanged(mDate);
  setNeedsPaint();
}

void DateField::nudge(const int days) {
  const auto next = std::chrono::sys_days{ mDate } + std::chrono::days{ days };
  const std::chrono::year_month_day date{ next };
  if (!date.ok()) return;
  setDate(date);
  if (mOnChanged) mOnChanged(mDate);
}

} // namespace coral::ui
