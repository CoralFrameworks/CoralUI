/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CORAL_UI_COLORPICKER_H
#define CORAL_UI_COLORPICKER_H

#include <ui/Types.h>
#include <ui/Layer.h>

#include <functional>
#include <vector>

namespace coral::ui {

struct Theme;
class Canvas;

enum class ColorPickerMode {
  Palette,
  Hsv,
};

/**
 * A compact palette picker.
 *
 * This is the dependency-free first step for colour selection: a grid of swatches with a single
 * selection. A full HSV/square picker can replace the body later without changing the callback
 * contract. The default palette is the editor's common accent/neutral colours.
 */
class ColorPicker final : public Layer {
 public:
  explicit ColorPicker(std::vector<Color> palette = {});
  ~ColorPicker() override;

  void setPalette(std::vector<Color> palette);
  [[nodiscard]] const std::vector<Color>& palette() const noexcept { return mPalette; }

  void setMode(ColorPickerMode mode);
  [[nodiscard]] ColorPickerMode mode() const noexcept { return mMode; }
  void setColor(Color color);
  [[nodiscard]] Color color() const noexcept;
  /** Alpha in [0,1]; only used by HSV mode. */
  void setAlpha(float alpha);
  [[nodiscard]] float alpha() const noexcept { return mAlpha; }

  void setSelected(int index);
  [[nodiscard]] int selected() const noexcept { return mSelected; }
  [[nodiscard]] Color selectedColor() const;

  void setOnChanged(std::function<void(Color)> callback);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "colorpicker";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  [[nodiscard]] int indexAt(Point point) const noexcept;
  [[nodiscard]] Rect cellRect(int index) const noexcept;
  [[nodiscard]] Rect hsvSquare() const noexcept;
  [[nodiscard]] Rect hueStrip() const noexcept;
  [[nodiscard]] Rect alphaStrip() const noexcept;
  void drawCheckerboard(Canvas& canvas, const Rect& bounds) const;
  void select(int index);
  void updateHsvFromPoint(Point point);
  void notify();
  void syncAccessibleValue();

  std::vector<Color> mPalette;
  std::function<void(Color)> mOnChanged;
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  float mCell = 24.0f;
  float mGap = 4.0f;
  float mRadius = 4.0f;
  int mColumns = 8;
  int mSelected = -1;
  ColorPickerMode mMode = ColorPickerMode::Palette;
  float mHue = 0.0f;
  float mSaturation = 0.0f;
  float mValue = 1.0f;
  float mAlpha = 1.0f;
  bool mDragging = false;
};

} // namespace coral::ui

#endif // CORAL_UI_COLORPICKER_H
