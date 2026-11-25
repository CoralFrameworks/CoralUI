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

#ifndef CORAL_UI_DATEFIELD_H
#define CORAL_UI_DATEFIELD_H

#include <ui/Types.h>

#include <ui/DateFormat.h>
#include <ui/Layer.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace coral::ui {

struct Theme;
class Calendar;
class Popup;
class TextInput;

/** A locale-formatted single-line date field. Up/Down moves by one day. */
class DateField final : public Layer {
 public:
  DateField(std::shared_ptr<const Font> font,
    std::chrono::year_month_day date = std::chrono::year_month_day{});
  ~DateField() override;

  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground, Color accent);
  void setFormat(const DateFormatOptions& format);
  [[nodiscard]] const DateFormatOptions& format() const noexcept {
    return mFormat;
  }
  void setDate(std::chrono::year_month_day date);
  [[nodiscard]] std::chrono::year_month_day date() const noexcept {
    return mDate;
  }
  /** The current edited text; useful to tests and copy/paste integration. */
  [[nodiscard]] std::string text() const;
  /** Open the native NSDatePicker panel on click (macOS; enabled by default). */
  void setNativePickerEnabled(bool enabled) {
    mNativePickerEnabled = enabled;
  }
  void setOnChanged(std::function<void(std::chrono::year_month_day)> callback) {
    mOnChanged = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  [[nodiscard]] bool acceptsTextInput() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  bool onTextInput(char32_t character) override;
  bool onTextComposition(const std::string& text) override;
  bool onTextCommit(const std::string& text) override;
  bool onTextDelete(int32_t index, uint32_t length) override;
  void onFocusChanged(bool focused) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "datefield";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  [[nodiscard]] std::string formatValue() const;
  bool parse(const std::string& text);
  void commit();
  void nudge(int days);
  void showCalendar();

  std::shared_ptr<const Font> mFont;
  std::shared_ptr<TextInput> mInput;
  DateFormatOptions mFormat = systemDateFormat();
  std::chrono::year_month_day mDate{};
  std::function<void(std::chrono::year_month_day)> mOnChanged;
  std::shared_ptr<Calendar> mCalendar;
  std::shared_ptr<Popup> mCalendarPopup;
  std::shared_ptr<bool> mCalendarAlive = std::make_shared<bool>(true);
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  float mPadding = 2.0f;
  float mRadius = 4.0f;
  bool mFocused = false;
  bool mNativePickerEnabled = false;
};

} // namespace coral::ui

#endif // CORAL_UI_DATEFIELD_H
