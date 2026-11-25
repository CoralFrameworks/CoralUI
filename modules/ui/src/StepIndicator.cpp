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

#include <ui/StepIndicator.h>

#include <graphics/Canvas.h>
#include <graphics/Paint.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui {

StepIndicator::StepIndicator(std::vector<std::string> steps,
    std::shared_ptr<const Font> font, const int current)
    : mSteps(std::move(steps)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Steps");
  rebuild();
  setCurrent(current);
}

void StepIndicator::setSteps(std::vector<std::string> steps) {
  mSteps = std::move(steps);
  rebuild();
  setCurrent(mCurrent);
  setNeedsLayout();
  setNeedsPaint();
}

void StepIndicator::setCurrent(const int current) {
  if (mSteps.empty()) {
    mCurrent = 0;
    setAccessibleValue({});
    return;
  }
  mCurrent = std::clamp(current, 0, static_cast<int>(mSteps.size()) - 1);
  setAccessibleValue(mSteps[static_cast<size_t>(mCurrent)]);
  for (size_t index = 0; index < mLabels.size(); ++index) {
    mLabels[index]->setColor(static_cast<int>(index) < mCurrent ? mCompleted
      : (static_cast<int>(index) == mCurrent ? mCurrentColor : mText));
  }
  setNeedsPaint();
}

void StepIndicator::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  rebuild();
  setNeedsLayout();
  setNeedsPaint();
}

Size StepIndicator::measure(const Size& available) {
  (void)available;
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  const float width = static_cast<float>(mSteps.size()) * stepWidth();
  setMeasuredSize({ width, mCircle + 6.0f + line });
  return measuredSize();
}

void StepIndicator::drawSelf(RenderContext& context) {
  if (mSteps.empty()) return;
  auto& canvas = context.canvas();
  const float width = stepWidth();
  const float centerY = bounds().getTop() + mCircle * 0.5f;
  graphics::Paint line;
  line.color = mPending;
  line.style = graphics::Paint::Style::kStroke;
  line.strokeWidth = 2.0f;
  if (mSteps.size() > 1) {
    canvas.drawLine(Point{ bounds().getLeft() + width * 0.5f, centerY },
      Point{ bounds().getLeft() + width * (static_cast<float>(mSteps.size()) - 0.5f), centerY },
      line);
  }
  for (size_t index = 0; index < mSteps.size(); ++index) {
    const float centerX = bounds().getLeft() + width * (static_cast<float>(index) + 0.5f);
    const bool done = static_cast<int>(index) < mCurrent;
    const bool active = static_cast<int>(index) == mCurrent;
    graphics::Paint fill;
    fill.color = done ? mCompleted : (active ? mCurrentColor : mPending);
    fill.style = graphics::Paint::Style::kFill;
    canvas.drawCircle(centerX, centerY, mCircle * 0.5f, fill);
    graphics::Paint border;
    border.color = active ? mCurrentColor : mPending;
    border.style = graphics::Paint::Style::kStroke;
    border.strokeWidth = 1.0f;
    canvas.drawCircle(centerX, centerY, mCircle * 0.5f, border);
  }
}

void StepIndicator::layout() {
  const float width = stepWidth();
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  for (size_t index = 0; index < mLabels.size(); ++index) {
    const float left = bounds().getLeft() + width * static_cast<float>(index);
    auto& label = mLabels[index];
    label->setBounds(Rect::MakeXYWH(left, bounds().getTop() + mCircle + 6.0f, width, line));
    label->setTextAlign(TextAlign::Center);
    label->layout();
    auto& number = mNumbers[index];
    number->setBounds(Rect::MakeXYWH(left + (width - mCircle) * 0.5f, bounds().getTop(),
      mCircle, mCircle));
    number->setTextAlign(TextAlign::Center);
    number->layout();
  }
  clearNeedsLayout();
}

bool StepIndicator::onPointerDown(const PointerEvent& event) {
  const int index = stepAt(event.position.x);
  if (index < 0) return false;
  setCurrent(index);
  if (mOnChanged) mOnChanged(index);
  return true;
}

void StepIndicator::applyTheme(const Theme& theme) {
  mCompleted = theme.controls.accent;
  mCurrentColor = theme.controls.accent;
  mPending = theme.controls.border;
  mText = theme.controls.foreground;
  for (size_t index = 0; index < mLabels.size(); ++index) {
    mLabels[index]->setColor(static_cast<int>(index) < mCurrent ? mCompleted
      : (static_cast<int>(index) == mCurrent ? mCurrentColor : mText));
  }
  setNeedsPaint();
}

void StepIndicator::rebuild() {
  for (const auto& label: mLabels) removeChild(label);
  for (const auto& number: mNumbers) removeChild(number);
  mLabels.clear();
  mNumbers.clear();
  for (size_t index = 0; index < mSteps.size(); ++index) {
    auto label = std::make_shared<TextLayer>(mSteps[index], mFont, mText);
    addChild(label);
    mLabels.push_back(std::move(label));
    auto number = std::make_shared<TextLayer>(std::to_string(index + 1), mFont,
      Color::MakeRGBA8(255, 255, 255, 255));
    addChild(number);
    mNumbers.push_back(std::move(number));
  }
  setNeedsLayout();
  setNeedsPaint();
}

float StepIndicator::stepWidth() const {
  float label = 60.0f;
  if (mFont != nullptr) {
    for (const auto& step: mSteps) label = std::max(label, mFont->measureWidth(step) + 12.0f);
  }
  return std::max(mCircle + 12.0f, std::min(label, 160.0f)) + mGap;
}

int StepIndicator::stepAt(const float x) const {
  if (mSteps.empty() || bounds().getWidth() <= 0.0f) return -1;
  const float width = stepWidth();
  const int index = static_cast<int>((x - bounds().getLeft()) / width);
  return index >= 0 && index < static_cast<int>(mSteps.size()) ? index : -1;
}

} // namespace coral::ui
