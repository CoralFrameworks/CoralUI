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

#ifndef CORAL_UI_SPLITVIEW_H
#define CORAL_UI_SPLITVIEW_H

#include <ui/Types.h>

#include <ui/Flex.h>
#include <ui/Layer.h>

#include <functional>
#include <memory>

namespace coral::ui {

struct Theme;

/**
 * Two panes separated by a draggable divider.
 *
 * The divider is outside both pane bounds, so a press on it reaches the SplitView. The window
 * captures the pointer for the drag, which keeps the divider moving even when the cursor leaves the
 * split line.
 */
class SplitView final : public Layer {
 public:
  explicit SplitView(Axis axis = Axis::Row, float fraction = 0.5f);

  void setPanes(std::shared_ptr<Layer> first, std::shared_ptr<Layer> second);
  [[nodiscard]] const std::shared_ptr<Layer>& first() const noexcept {
    return mFirst;
  }
  [[nodiscard]] const std::shared_ptr<Layer>& second() const noexcept {
    return mSecond;
  }

  void setFraction(float fraction);
  [[nodiscard]] float fraction() const noexcept {
    return mFraction;
  }
  void setDividerWidth(float width);
  void setMinimumPaneSize(float size);
  void setOnFractionChanged(std::function<void(float)> callback) {
    mOnFractionChanged = std::move(callback);
  }

  [[nodiscard]] bool capturesPointerOnPress() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  [[nodiscard]] Rect dividerBounds() const;
  void updateFromPointer(const Point& point);

  Axis mAxis = Axis::Row;
  std::shared_ptr<Layer> mFirst;
  std::shared_ptr<Layer> mSecond;
  std::function<void(float)> mOnFractionChanged;
  float mFraction = 0.5f;
  float mDividerWidth = 6.0f;
  float mMinimumPaneSize = 80.0f;
  Color mDividerColor = Color::MakeRGBA8(62, 67, 78, 255);
  bool mDragging = false;
};

} // namespace coral::ui

#endif // CORAL_UI_SPLITVIEW_H
