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

#include <ui/ColorPicker.h>

#include <ui/RenderContext.h>
#include <ui/Theme.h>

#include "WidgetDrawing.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <cmath>
#include <utility>

namespace coral::ui {

namespace {

std::vector<Color> defaultPalette() {
  return {
    Color::MakeRGBA8(255, 255, 255, 255),
    Color::MakeRGBA8(226, 232, 240, 255),
    Color::MakeRGBA8(148, 163, 184, 255),
    Color::MakeRGBA8(90, 96, 108, 255),
    Color::MakeRGBA8(35, 38, 46, 255),
    Color::MakeRGBA8(0, 0, 0, 255),
    Color::MakeRGBA8(86, 156, 255, 255),
    Color::MakeRGBA8(56, 132, 220, 255),
    Color::MakeRGBA8(56, 189, 248, 255),
    Color::MakeRGBA8(45, 212, 191, 255),
    Color::MakeRGBA8(74, 222, 128, 255),
    Color::MakeRGBA8(250, 204, 21, 255),
    Color::MakeRGBA8(251, 146, 60, 255),
    Color::MakeRGBA8(248, 113, 113, 255),
    Color::MakeRGBA8(244, 114, 182, 255),
    Color::MakeRGBA8(192, 132, 252, 255),
  };
}

std::array<float, 3> rgbToHsv(const Color color) {
  const float maximum = std::max({ color.red, color.green, color.blue });
  const float minimum = std::min({ color.red, color.green, color.blue });
  const float delta = maximum - minimum;
  float hue = 0.0f;
  if (delta > 0.00001f) {
    if (maximum == color.red) hue = 60.0f * std::fmod((color.green - color.blue) / delta, 6.0f);
    else if (maximum == color.green) hue = 60.0f * ((color.blue - color.red) / delta + 2.0f);
    else hue = 60.0f * ((color.red - color.green) / delta + 4.0f);
    if (hue < 0.0f) hue += 360.0f;
  }
  const float saturation = maximum <= 0.00001f ? 0.0f : delta / maximum;
  return { hue, saturation, maximum };
}

Color hsvToColor(const float hue, const float saturation, const float value, const float alpha) {
  const float h = std::fmod(std::max(0.0f, hue), 360.0f);
  const float s = std::clamp(saturation, 0.0f, 1.0f);
  const float v = std::clamp(value, 0.0f, 1.0f);
  const float chroma = v * s;
  const float x = chroma * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
  const float m = v - chroma;
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  if (h < 60.0f) { red = chroma; green = x; }
  else if (h < 120.0f) { red = x; green = chroma; }
  else if (h < 180.0f) { green = chroma; blue = x; }
  else if (h < 240.0f) { green = x; blue = chroma; }
  else if (h < 300.0f) { red = x; blue = chroma; }
  else { red = chroma; blue = x; }
  return Color{ red + m, green + m, blue + m, alpha };
}

} // namespace

ColorPicker::ColorPicker(std::vector<Color> palette)
    : mPalette(palette.empty() ? defaultPalette() : std::move(palette)) {
  setAccessibleRole(AccessibilityRole::ColorPicker);
  setAccessibleName("Color");
  syncAccessibleValue();
}

ColorPicker::~ColorPicker() = default;

void ColorPicker::setPalette(std::vector<Color> palette) {
  mPalette = palette.empty() ? defaultPalette() : std::move(palette);
  mSelected = std::min(mSelected, static_cast<int>(mPalette.size()) - 1);
  syncAccessibleValue();
  setNeedsLayout();
  setNeedsPaint();
}

void ColorPicker::setMode(const ColorPickerMode mode) {
  if (mMode == mode) return;
  mMode = mode;
  if (mMode == ColorPickerMode::Hsv && mSelected >= 0 &&
      mSelected < static_cast<int>(mPalette.size())) {
    setColor(mPalette[static_cast<size_t>(mSelected)]);
  }
  setNeedsLayout();
  setNeedsPaint();
}

void ColorPicker::setColor(const Color color) {
  const auto hsv = rgbToHsv(color);
  mHue = hsv[0];
  mSaturation = hsv[1];
  mValue = hsv[2];
  mAlpha = color.alpha;
  syncAccessibleValue();
  setNeedsPaint();
}

void ColorPicker::setAlpha(const float alpha) {
  mAlpha = std::clamp(alpha, 0.0f, 1.0f);
  syncAccessibleValue();
  setNeedsPaint();
}

Color ColorPicker::color() const noexcept {
  if (mMode == ColorPickerMode::Hsv) {
    return hsvToColor(mHue, mSaturation, mValue, mAlpha);
  }
  if (mSelected < 0 || mSelected >= static_cast<int>(mPalette.size())) return Color{};
  return mPalette[static_cast<size_t>(mSelected)];
}

void ColorPicker::setSelected(const int index) {
  if (index < 0 || index >= static_cast<int>(mPalette.size())) return;
  mSelected = index;
  if (mMode == ColorPickerMode::Hsv) setColor(mPalette[static_cast<size_t>(index)]);
  syncAccessibleValue();
  setNeedsPaint();
}

Color ColorPicker::selectedColor() const {
  return color();
}

void ColorPicker::setOnChanged(std::function<void(Color)> callback) {
  mOnChanged = std::move(callback);
}

Size ColorPicker::measure(const Size& available) {
  if (mMode == ColorPickerMode::Hsv) {
    const float width = available.width > 0.0f ? std::min(available.width, 200.0f) : 200.0f;
    const float height = available.height > 0.0f ? std::min(available.height, 220.0f) : 220.0f;
    setMeasuredSize({ width, height });
    return measuredSize();
  }
  const int count = std::max(1, static_cast<int>(mPalette.size()));
  const int columns = std::min(mColumns, count);
  const int rows = (count + mColumns - 1) / mColumns;
  const float width = static_cast<float>(columns) * mCell + static_cast<float>(columns - 1) * mGap;
  const float height = static_cast<float>(rows) * mCell + static_cast<float>(rows - 1) * mGap;
  setMeasuredSize({ available.width > 0.0f ? std::min(available.width, width) : width,
    available.height > 0.0f ? std::min(available.height, height) : height });
  return measuredSize();
}

Rect ColorPicker::cellRect(const int index) const noexcept {
  const int column = index % mColumns;
  const int row = index / mColumns;
  return Rect::MakeXYWH(bounds().getLeft() + static_cast<float>(column) * (mCell + mGap),
    bounds().getTop() + static_cast<float>(row) * (mCell + mGap), mCell, mCell);
}

Rect ColorPicker::hsvSquare() const noexcept {
  const float side = std::max(1.0f, std::min(bounds().getWidth(), bounds().getHeight() - 44.0f));
  return Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), side, side);
}

Rect ColorPicker::hueStrip() const noexcept {
  const float side = hsvSquare().getWidth();
  return Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + side + 10.0f, side, 14.0f);
}

Rect ColorPicker::alphaStrip() const noexcept {
  const Rect hue = hueStrip();
  return Rect::MakeXYWH(bounds().getLeft(), hue.getBottom() + 6.0f, hue.getWidth(), 14.0f);
}

void ColorPicker::drawCheckerboard(Canvas& canvas, const Rect& area) const {
  constexpr float kSquare = 8.0f;
  const Color light = Color::MakeRGBA8(210, 214, 220, 255);
  const Color dark = Color::MakeRGBA8(160, 166, 176, 255);
  const int columns = std::max(1, static_cast<int>(std::ceil(area.getWidth() / kSquare)));
  const int rows = std::max(1, static_cast<int>(std::ceil(area.getHeight() / kSquare)));
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      Paint paint;
      paint.style = Paint::Style::kFill;
      paint.color = ((row + column) & 1) == 0 ? light : dark;
      canvas.drawRect(Rect::MakeXYWH(area.getLeft() + static_cast<float>(column) * kSquare,
        area.getTop() + static_cast<float>(row) * kSquare, kSquare, kSquare), paint);
    }
  }
}

int ColorPicker::indexAt(const Point point) const noexcept {
  for (int index = 0; index < static_cast<int>(mPalette.size()); ++index) {
    if (cellRect(index).contains(point)) return index;
  }
  return -1;
}

void ColorPicker::drawSelf(RenderContext& context) {
  Canvas& canvas = context.canvas();
  if (mMode == ColorPickerMode::Hsv) {
    const Rect square = hsvSquare();
    constexpr int kSteps = 16;
    const float stepX = square.getWidth() / static_cast<float>(kSteps);
    const float stepY = square.getHeight() / static_cast<float>(kSteps);
    for (int row = 0; row < kSteps; ++row) {
      for (int column = 0; column < kSteps; ++column) {
        Paint cell;
        cell.style = Paint::Style::kFill;
        cell.color = hsvToColor(mHue, static_cast<float>(column) / static_cast<float>(kSteps - 1),
          1.0f - static_cast<float>(row) / static_cast<float>(kSteps - 1), 1.0f);
        canvas.drawRect(Rect::MakeXYWH(square.getLeft() + static_cast<float>(column) * stepX,
          square.getTop() + static_cast<float>(row) * stepY, stepX + 0.5f, stepY + 0.5f), cell);
      }
    }
    const Rect hue = hueStrip();
    constexpr int kHueSteps = 48;
    for (int index = 0; index < kHueSteps; ++index) {
      Paint cell;
      cell.style = Paint::Style::kFill;
      cell.color = hsvToColor(static_cast<float>(index) * 360.0f / static_cast<float>(kHueSteps),
        1.0f, 1.0f, 1.0f);
      canvas.drawRect(Rect::MakeXYWH(hue.getLeft() + static_cast<float>(index) * hue.getWidth() /
        static_cast<float>(kHueSteps), hue.getTop(), hue.getWidth() /
        static_cast<float>(kHueSteps) + 0.5f, hue.getHeight()), cell);
    }
    const Point marker{ square.getLeft() + mSaturation * square.getWidth(),
      square.getTop() + (1.0f - mValue) * square.getHeight() };
    Paint ring;
    ring.color = Color::MakeRGBA8(255, 255, 255, 255);
    ring.style = Paint::Style::kStroke;
    ring.strokeWidth = 2.0f;
    canvas.drawCircle(marker.x, marker.y, 4.0f, ring);
    const Point hueMarker{ hue.getLeft() + (mHue / 360.0f) * hue.getWidth(), hue.getCenter().y };
    canvas.drawCircle(hueMarker.x, hueMarker.y, 5.0f, ring);

    const Rect alpha = alphaStrip();
    drawCheckerboard(canvas, alpha);
    constexpr int kAlphaSteps = 48;
    for (int index = 0; index < kAlphaSteps; ++index) {
      Paint cell;
      cell.style = Paint::Style::kFill;
      cell.color = hsvToColor(mHue, mSaturation, mValue,
        static_cast<float>(index) / static_cast<float>(kAlphaSteps - 1));
      canvas.drawRect(Rect::MakeXYWH(alpha.getLeft() + static_cast<float>(index) *
        alpha.getWidth() / static_cast<float>(kAlphaSteps), alpha.getTop(),
        alpha.getWidth() / static_cast<float>(kAlphaSteps) + 0.5f, alpha.getHeight()), cell);
    }
    const Point alphaMarker{ alpha.getLeft() + mAlpha * alpha.getWidth(), alpha.getCenter().y };
    canvas.drawCircle(alphaMarker.x, alphaMarker.y, 5.0f, ring);
    return;
  }

  for (int index = 0; index < static_cast<int>(mPalette.size()); ++index) {
    const Rect cell = cellRect(index);
    Paint fill;
    fill.color = mPalette[static_cast<size_t>(index)];
    fill.style = Paint::Style::kFill;
    canvas.drawRoundRect(RoundRect::MakeRectRadius(cell, mRadius), fill);
    Paint border;
    border.color = mBorder;
    border.style = Paint::Style::kStroke;
    border.strokeWidth = 1.0f;
    canvas.drawRoundRect(RoundRect::MakeRectRadius(cell, mRadius), border);
    if (index == mSelected) {
      detail::drawFocusRing(canvas, cell, true, mFocus, mRadius, 2.0f);
    }
  }
}

bool ColorPicker::onPointerDown(const PointerEvent& event) {
  if (event.button != PointerButton::Primary) return false;
  if (mMode == ColorPickerMode::Hsv) {
    if (!hsvSquare().contains(event.position) && !hueStrip().contains(event.position) &&
        !alphaStrip().contains(event.position)) return false;
    mDragging = true;
    updateHsvFromPoint(event.position);
    return true;
  }
  const int index = indexAt(event.position);
  if (index < 0) return false;
  select(index);
  return true;
}

bool ColorPicker::onPointerMove(const PointerEvent& event) {
  if (!mDragging || mMode != ColorPickerMode::Hsv) return false;
  updateHsvFromPoint(event.position);
  return true;
}

bool ColorPicker::onPointerUp(const PointerEvent& event) {
  if (event.button != PointerButton::Primary) return false;
  const bool wasDragging = mDragging;
  mDragging = false;
  return wasDragging;
}

void ColorPicker::applyTheme(const Theme& theme) {
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void ColorPicker::select(const int index) {
  if (mSelected == index) return;
  mSelected = index;
  syncAccessibleValue();
  setNeedsPaint();
  notify();
}

void ColorPicker::updateHsvFromPoint(const Point point) {
  const Rect square = hsvSquare();
  if (square.contains(point)) {
    mSaturation = std::clamp((point.x - square.getLeft()) / square.getWidth(), 0.0f, 1.0f);
    mValue = 1.0f - std::clamp((point.y - square.getTop()) / square.getHeight(), 0.0f, 1.0f);
  } else {
    const Rect hue = hueStrip();
    if (hue.contains(point)) {
      mHue = std::clamp((point.x - hue.getLeft()) / hue.getWidth(), 0.0f, 1.0f) * 360.0f;
    } else {
      const Rect alpha = alphaStrip();
      if (alpha.contains(point)) {
        mAlpha = std::clamp((point.x - alpha.getLeft()) / alpha.getWidth(), 0.0f, 1.0f);
      }
    }
  }
  notify();
}

void ColorPicker::syncAccessibleValue() {
  if (mMode == ColorPickerMode::Palette &&
      (mSelected < 0 || mSelected >= static_cast<int>(mPalette.size()))) {
    setAccessibleValue({});
    return;
  }
  const Color color = this->color();
  char text[16]{};
  std::snprintf(text, sizeof(text), "#%02X%02X%02X",
    static_cast<int>(std::lround(color.red * 255.0f)),
    static_cast<int>(std::lround(color.green * 255.0f)),
    static_cast<int>(std::lround(color.blue * 255.0f)));
  setAccessibleValue(text);
}

bool ColorPicker::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::SetValue) return false;
  std::string text(value);
  if (text.empty() || text.front() != '#') return false;
  text.erase(text.begin());
  if (text.size() != 6 && text.size() != 8) return false;
  unsigned channels[4] = { 255, 255, 255, 255 };
  for (size_t index = 0; index < text.size() / 2; ++index) {
    char* end = nullptr;
    const std::string part = text.substr(index * 2, 2);
    const long channel = std::strtol(part.c_str(), &end, 16);
    if (end == part.c_str() || *end != '\0' || channel < 0 || channel > 255) return false;
    channels[index] = static_cast<unsigned>(channel);
  }
  const Color requested = Color::MakeRGBA8(static_cast<uint8_t>(channels[0]),
    static_cast<uint8_t>(channels[1]), static_cast<uint8_t>(channels[2]),
    static_cast<uint8_t>(channels[3]));
  if (mMode == ColorPickerMode::Palette) {
    for (size_t index = 0; index < mPalette.size(); ++index) {
      const Color& candidate = mPalette[index];
      if (std::abs(candidate.red - requested.red) < 1e-4f &&
          std::abs(candidate.green - requested.green) < 1e-4f &&
          std::abs(candidate.blue - requested.blue) < 1e-4f) {
        mSelected = static_cast<int>(index);
        syncAccessibleValue();
        setNeedsPaint();
        notify();
        return true;
      }
    }
    setMode(ColorPickerMode::Hsv);
  }
  setColor(requested);
  notify();
  return true;
}

void ColorPicker::notify() {
  setNeedsPaint();
  if (mOnChanged != nullptr) mOnChanged(color());
}

} // namespace coral::ui
