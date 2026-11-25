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

#ifndef CORAL_UI_CALENDAR_H
#define CORAL_UI_CALENDAR_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <array>
#include <chrono>
#include <functional>
#include <memory>

namespace coral::ui {

struct Theme;

/** A cross-platform month calendar used by DateField and date pickers. */
class Calendar final : public Layer {
 public:
  Calendar(std::chrono::year_month_day selected, std::shared_ptr<const Font> font);

  void setDate(std::chrono::year_month_day date);
  [[nodiscard]] std::chrono::year_month_day date() const noexcept {
    return mSelected;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground, Color accent, Color muted);
  /** 0 = Sunday ... 6 = Saturday; defaults to the system locale's first weekday. */
  void setFirstWeekday(int firstWeekday);
  [[nodiscard]] int firstWeekday() const noexcept {
    return mFirstWeekday;
  }
  /** Override the twelve month names (for example with localized strings). */
  void setMonthNames(std::array<std::string, 12> names);
  void setOnSelect(std::function<void(std::chrono::year_month_day)> callback) {
    mOnSelect = std::move(callback);
  }
  void setOnCancel(std::function<void()> callback) {
    mOnCancel = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "calendar";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void changeMonth(int delta);
  void selectDay(std::chrono::year_month_day date);
  [[nodiscard]] int firstSlot() const;
  [[nodiscard]] int daysInMonth() const;

  std::chrono::year_month_day mSelected{};
  int mViewYear = 1970;
  unsigned mViewMonth = 1;
  std::shared_ptr<const Font> mFont;
  std::function<void(std::chrono::year_month_day)> mOnSelect;
  std::function<void()> mOnCancel;
  Color mBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  Color mMuted = Color::MakeRGBA8(130, 140, 155, 255);
  std::array<std::string, 12> mMonthNames;
  std::array<std::string, 7> mWeekdayNames;
  int mFirstWeekday = 0;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_CALENDAR_H
