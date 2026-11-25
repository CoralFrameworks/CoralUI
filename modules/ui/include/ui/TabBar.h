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

#ifndef CORAL_UI_TABBAR_H
#define CORAL_UI_TABBAR_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;

/**
 * A tab strip with one visible page per tab.
 *
 * The header is laid out from measured label widths; the active page occupies the area below it.
 * Clicking a tab or pressing Left/Right while the bar is focused switches pages.
 */
class TabBar final : public Layer {
 public:
  explicit TabBar(std::shared_ptr<const Font> font);
  ~TabBar() override;

  int addTab(std::string label, std::shared_ptr<Layer> content);
  void clear();
  [[nodiscard]] size_t tabCount() const noexcept {
    return mTabs.size();
  }

  void setActiveTab(int index);
  [[nodiscard]] int activeIndex() const noexcept {
    return mActive;
  }
  [[nodiscard]] std::shared_ptr<Layer> content(int index) const;

  void setOnChanged(std::function<void(int)> callback) {
    mOnChanged = std::move(callback);
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  void onFocusChanged(bool focused) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;

  [[nodiscard]] const char* defaultStyleName() const override {
    return "tabbar";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  struct Tab {
    std::string label;
    std::shared_ptr<Layer> content;
    std::shared_ptr<Layer> accessibilityItem;
    Rect bounds{};
  };

  [[nodiscard]] int tabAt(const Point& point) const;
  [[nodiscard]] float tabWidth(const std::string& label) const;
  void updateVisibility();

  std::shared_ptr<const Font> mFont;
  std::vector<Tab> mTabs;
  std::function<void(int)> mOnChanged;
  int mActive = -1;
  float mHeaderHeight = 36.0f;
  float mTabPadding = 12.0f;
  float mGap = 0.0f;
  float mRadius = 4.0f;
  Color mBackground = Color::MakeRGBA8(24, 26, 32, 255);
  Color mTabBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mActiveBackground = Color::MakeRGBA8(52, 56, 66, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_TABBAR_H
