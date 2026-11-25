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

#ifndef CORAL_UI_TIMEFIELD_H
#define CORAL_UI_TIMEFIELD_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace coral::ui {

struct Theme;
class Popup;
class TextInput;
class TimePicker;

/** A cross-platform single-line time field with 24-hour and 12-hour (AM/PM) forms. */
class TimeField final : public Layer {
 public:
  /** Omit @p use24HourClock to follow the current system locale. */
  TimeField(std::shared_ptr<const Font> font, int hour = 0, int minute = 0,
    std::optional<bool> use24HourClock = std::nullopt);
  ~TimeField() override;

  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground, Color accent);
  void setTime(int hour, int minute);
  [[nodiscard]] int hour() const noexcept {
    return mHour;
  }
  [[nodiscard]] int minute() const noexcept {
    return mMinute;
  }
  void setUse24HourClock(bool use24Hour);
  [[nodiscard]] bool uses24HourClock() const noexcept {
    return mUse24HourClock;
  }
  [[nodiscard]] std::string text() const;
  void setOnChanged(std::function<void(int, int)> callback) {
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
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "timefield";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  [[nodiscard]] std::string formatValue() const;
  bool parse(const std::string& text);
  void commit();
  void nudge(int minutes);
  void showTimePicker();

  std::shared_ptr<const Font> mFont;
  std::shared_ptr<TextInput> mInput;
  std::function<void(int, int)> mOnChanged;
  std::shared_ptr<TimePicker> mTimePicker;
  std::shared_ptr<Popup> mPickerPopup;
  std::shared_ptr<bool> mPickerAlive = std::make_shared<bool>(true);
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  float mPadding = 2.0f;
  float mRadius = 4.0f;
  int mHour = 0;
  int mMinute = 0;
  bool mUse24HourClock = true;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_TIMEFIELD_H
