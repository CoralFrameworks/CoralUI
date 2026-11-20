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

#include <ui/ColorInput.h>

#include <ui/TextInput.h>
#include <ui/Theme.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <utility>

namespace coral::ui {

namespace {

std::optional<int> parseInteger(const std::string& text, const int base, const int maximum) {
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  const long value = std::strtol(text.c_str(), &end, base);
  if (end == text.c_str() || *end != '\0' || value < 0 || value > maximum) return std::nullopt;
  return static_cast<int>(value);
}

std::optional<Color> parseHex(std::string text) {
  if (!text.empty() && text.front() == '#') text.erase(text.begin());
  for (char& character: text) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  if (text.size() == 3) {
    std::string expanded;
    expanded.reserve(6);
    for (const char character: text) {
      expanded.push_back(character);
      expanded.push_back(character);
    }
    text = std::move(expanded);
  }
  if (text.size() != 6 && text.size() != 8) return std::nullopt;
  const auto red = parseInteger(text.substr(0, 2), 16, 255);
  const auto green = parseInteger(text.substr(2, 2), 16, 255);
  const auto blue = parseInteger(text.substr(4, 2), 16, 255);
  if (!red || !green || !blue) return std::nullopt;
  int alpha = 255;
  if (text.size() == 8) {
    const auto parsed = parseInteger(text.substr(6, 2), 16, 255);
    if (!parsed) return std::nullopt;
    alpha = *parsed;
  }
  return Color::MakeRGBA8(static_cast<uint8_t>(*red), static_cast<uint8_t>(*green),
    static_cast<uint8_t>(*blue), static_cast<uint8_t>(alpha));
}

std::string hexString(const Color color) {
  const auto channel = [](const float value) {
    return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
  };
  char buffer[10]{};
  if (channel(color.alpha) == 255) {
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", channel(color.red),
      channel(color.green), channel(color.blue));
  } else {
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", channel(color.red),
      channel(color.green), channel(color.blue), channel(color.alpha));
  }
  return buffer;
}

std::string channelString(const float value) {
  return std::to_string(static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)));
}

} // namespace

ColorInput::ColorInput(std::shared_ptr<const Font> font, const Color color) : mColor(color) {
  const std::shared_ptr<const Font> fieldFont = font;
  mHex = std::make_shared<TextInput>("", fieldFont);
  mRed = std::make_shared<TextInput>("", fieldFont);
  mGreen = std::make_shared<TextInput>("", fieldFont);
  mBlue = std::make_shared<TextInput>("", fieldFont);
  mHex->setPlaceholder("#RRGGBB");
  for (const auto& field: { mRed, mGreen, mBlue }) field->setPlaceholder("0");
  addChild(mHex);
  addChild(mRed);
  addChild(mGreen);
  addChild(mBlue);
  mObservations.push_back(mHex->observeChanged([this](const std::string&) {
    if (!mSyncing) readHex();
  }));
  for (const auto& field: { mRed, mGreen, mBlue }) {
    mObservations.push_back(field->observeChanged([this](const std::string&) {
      if (!mSyncing) readRgb();
    }));
  }
  syncFields();
}

ColorInput::~ColorInput() = default;

void ColorInput::setFont(std::shared_ptr<const Font> font) {
  mHex->setFont(font);
  mRed->setFont(font);
  mGreen->setFont(font);
  mBlue->setFont(font);
  setNeedsLayout();
}

void ColorInput::setColor(const Color color) {
  mColor = color;
  syncFields();
}

void ColorInput::setOnChanged(std::function<void(Color)> callback) {
  mOnChanged = std::move(callback);
}

Size ColorInput::measure(const Size& available) {
  const float width = available.width > 0.0f ? std::min(available.width, 256.0f) : 256.0f;
  setMeasuredSize({ width, std::max(26.0f, mHex->measure({ 0.0f, 0.0f }).height) });
  return measuredSize();
}

void ColorInput::layout() {
  constexpr float gap = 4.0f;
  const float hexWidth = std::max(80.0f, bounds().getWidth() - 3.0f * (44.0f + gap));
  const float channelWidth = 44.0f;
  const float y = bounds().getTop();
  const float height = std::max(24.0f, bounds().getHeight());
  mHex->setBounds(Rect::MakeXYWH(bounds().getLeft(), y, hexWidth, height));
  mRed->setBounds(Rect::MakeXYWH(bounds().getLeft() + hexWidth + gap, y, channelWidth, height));
  mGreen->setBounds(Rect::MakeXYWH(bounds().getLeft() + hexWidth + gap * 2.0f + channelWidth, y,
    channelWidth, height));
  mBlue->setBounds(Rect::MakeXYWH(bounds().getLeft() + hexWidth + gap * 3.0f + channelWidth * 2.0f,
    y, channelWidth, height));
  clearNeedsLayout();
}

void ColorInput::applyTheme(const Theme& theme) {
  for (const auto& field: { mHex, mRed, mGreen, mBlue }) {
    field->setColors(theme.controls.background, theme.controls.foreground);
  }
}

void ColorInput::syncFields() {
  mSyncing = true;
  mHex->setText(hexString(mColor));
  mRed->setText(channelString(mColor.red));
  mGreen->setText(channelString(mColor.green));
  mBlue->setText(channelString(mColor.blue));
  mSyncing = false;
}

void ColorInput::readHex() {
  const std::optional<Color> parsed = parseHex(mHex->text());
  if (!parsed.has_value()) return;
  mColor = *parsed;
  mSyncing = true;
  mRed->setText(channelString(mColor.red));
  mGreen->setText(channelString(mColor.green));
  mBlue->setText(channelString(mColor.blue));
  mSyncing = false;
  notify();
}

void ColorInput::readRgb() {
  const auto red = parseInteger(mRed->text(), 10, 255);
  const auto green = parseInteger(mGreen->text(), 10, 255);
  const auto blue = parseInteger(mBlue->text(), 10, 255);
  if (!red || !green || !blue) return;
  mColor = Color::MakeRGBA8(static_cast<uint8_t>(*red), static_cast<uint8_t>(*green),
    static_cast<uint8_t>(*blue), static_cast<uint8_t>(std::lround(mColor.alpha * 255.0f)));
  mSyncing = true;
  mHex->setText(hexString(mColor));
  mSyncing = false;
  notify();
}

void ColorInput::notify() {
  if (mOnChanged != nullptr) mOnChanged(mColor);
}

void ColorInput::setFieldText(TextInput& field, const std::string& text) {
  if (field.text() != text) field.setText(text);
}

} // namespace coral::ui
