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

#ifndef CORAL_UI_BREADCRUMB_H
#define CORAL_UI_BREADCRUMB_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace coral::ui {

struct Theme;

/** A cross-platform breadcrumb trail; clicking a crumb reports its index and text. */
class Breadcrumb final : public Layer {
 public:
  using SelectHandler = std::function<void(int, std::string_view)>;

  Breadcrumb(std::vector<std::string> items, std::shared_ptr<const Font> font);

  void setItems(std::vector<std::string> items);
  [[nodiscard]] const std::vector<std::string>& items() const noexcept {
    return mItems;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setSeparator(std::string separator);
  void setColors(Color foreground, Color accent);
  void setOnSelect(SelectHandler callback) {
    mOnSelect = std::move(callback);
  }
  void setCurrentIndex(int index);
  [[nodiscard]] int currentIndex() const noexcept {
    return mCurrent;
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "breadcrumb";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  [[nodiscard]] std::vector<Rect> itemRects() const;

  std::vector<std::string> mItems;
  std::shared_ptr<const Font> mFont;
  std::string mSeparator = "/";
  SelectHandler mOnSelect;
  Color mForeground = Color::MakeRGBA8(180, 190, 205, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  float mGap = 8.0f;
  int mCurrent = -1;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_BREADCRUMB_H
