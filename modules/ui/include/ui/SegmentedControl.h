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

#ifndef CORAL_UI_SEGMENTEDCONTROL_H
#define CORAL_UI_SEGMENTEDCONTROL_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;

/**
 * A row of mutually exclusive text segments (a segmented control / tab strip without pages).
 *
 * The selected index is the model value; clicking a segment or pressing Left/Right when focused
 * reports through `setOnChanged`.
 */
class SegmentedControl final : public Layer {
 public:
  SegmentedControl(std::vector<std::string> segments,
    std::shared_ptr<const Font> font);

  void setSegments(std::vector<std::string> segments);
  [[nodiscard]] const std::vector<std::string>& segments() const noexcept {
    return mSegments;
  }
  void setFont(std::shared_ptr<const Font> font);
  void setColors(Color background, Color foreground,
    Color accent);
  void setSelectedIndex(int index);
  [[nodiscard]] int selectedIndex() const noexcept {
    return mSelected;
  }
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
  [[nodiscard]] const char* defaultStyleName() const override { return "segmented"; }
  [[nodiscard]] StateMask styleStates() const override {
    StateMask mask = Layer::styleStates() | stateFlag(ControlState::Selected);
    if (mFocused) mask |= stateFlag(ControlState::Focused);
    return mask;
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void select(int index);
  void rebuildAccessibilityItems();
  [[nodiscard]] float segmentWidth(const std::string& label) const;

  std::vector<std::string> mSegments;
  std::vector<std::shared_ptr<Layer>> mAccessibilityItems;
  std::shared_ptr<const Font> mFont;
  Color mBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mDivider = Color::MakeRGBA8(62, 68, 80, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mAccent = Color::MakeRGBA8(86, 156, 255, 255);
  std::function<void(int)> mOnChanged;
  float mSegmentPadding = 12.0f;
  float mRadius = 4.0f;
  int mSelected = 0;
  bool mFocused = false;
};

} // namespace coral::ui

#endif // CORAL_UI_SEGMENTEDCONTROL_H
