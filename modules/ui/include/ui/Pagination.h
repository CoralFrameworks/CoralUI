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

#ifndef CORAL_UI_PAGINATION_H
#define CORAL_UI_PAGINATION_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <memory>

namespace coral::ui {

struct Theme;

/** Previous/next paging with a five-page window. */
class Pagination final : public Layer {
 public:
  Pagination(std::shared_ptr<const Font> font, int pageCount = 1, int page = 0);

  void setPageCount(int pageCount);
  [[nodiscard]] int pageCount() const noexcept {
    return mPageCount;
  }
  void setPage(int page);
  [[nodiscard]] int page() const noexcept {
    return mPage;
  }
  void setOnChanged(std::function<void(int)> callback) {
    mOnChanged = std::move(callback);
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
    return "pagination";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  [[nodiscard]] int firstPage() const;
  [[nodiscard]] int visibleCount() const;
  [[nodiscard]] int indexAt(Point point) const;
  void choose(int page);

  std::shared_ptr<const Font> mFont;
  std::function<void(int)> mOnChanged;
  Color mBackground = Color::MakeRGBA8(45, 49, 59, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  Color mBorder = Color::MakeRGBA8(70, 76, 88, 255);
  float mItem = 28.0f;
  float mGap = 4.0f;
  float mRadius = 4.0f;
  int mPageCount = 1;
  int mPage = 0;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_PAGINATION_H
