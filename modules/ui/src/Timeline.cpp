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

#include <ui/Timeline.h>

#include <graphics/Canvas.h>
#include <graphics/Paint.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

Timeline::Timeline(std::vector<TimelineEntry> entries, std::shared_ptr<const Font> font)
    : mEntries(std::move(entries)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::Group);
  setAccessibleName("Timeline");
  setAccessibleValue(std::to_string(mEntries.size()));
  rebuild();
}

void Timeline::setEntries(std::vector<TimelineEntry> entries) {
  mEntries = std::move(entries);
  setAccessibleValue(std::to_string(mEntries.size()));
  rebuild();
}

void Timeline::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  rebuild();
}

Size Timeline::measure(const Size& available) {
  (void)available;
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  float width = 200.0f;
  if (mFont != nullptr) {
    for (const auto& entry: mEntries) {
      width = std::max(width, mFont->measureWidth(entry.title) + 40.0f);
      width = std::max(width, mFont->measureWidth(entry.detail) + 40.0f);
    }
  }
  setMeasuredSize({ width, static_cast<float>(mEntries.size()) * (line * 2.0f + 10.0f) });
  return measuredSize();
}

void Timeline::layout() {
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  const float rowHeight = line * 2.0f + 10.0f;
  for (size_t index = 0; index < mRows.size(); ++index) {
    const float top = bounds().getTop() + static_cast<float>(index) * rowHeight;
    const float left = bounds().getLeft() + 30.0f;
    const float width = std::max(0.0f, bounds().getWidth() - 30.0f);
    mRows[index].text->setBounds(Rect::MakeXYWH(left, top, width, line));
    mRows[index].detail->setBounds(Rect::MakeXYWH(left, top + line + 2.0f, width, line));
    mRows[index].text->layout();
    mRows[index].detail->layout();
    if (mRows[index].time != nullptr) {
      mRows[index].time->setBounds(Rect::MakeXYWH(bounds().getRight() - 80.0f, top,
        80.0f, line));
      mRows[index].time->layout();
    }
  }
  clearNeedsLayout();
}

void Timeline::drawSelf(RenderContext& context) {
  const float line = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent : 18.0f;
  const float rowHeight = line * 2.0f + 10.0f;
  if (mEntries.empty()) return;
  auto& canvas = context.canvas();
  graphics::Paint connector;
  connector.color = mPending;
  connector.style = graphics::Paint::Style::kStroke;
  connector.strokeWidth = 2.0f;
  canvas.drawLine(Point{ bounds().getLeft() + 10.0f, bounds().getTop() + 8.0f },
    Point{ bounds().getLeft() + 10.0f, bounds().getTop() + rowHeight *
      static_cast<float>(mEntries.size() - 1) + 8.0f }, connector);
  for (size_t index = 0; index < mEntries.size(); ++index) {
    const float centerY = bounds().getTop() + static_cast<float>(index) * rowHeight + 8.0f;
    graphics::Paint dot;
    dot.color = mEntries[index].state == TimelineState::Complete ? mComplete
      : (mEntries[index].state == TimelineState::Current ? mCurrent : mPending);
    dot.style = graphics::Paint::Style::kFill;
    canvas.drawCircle(bounds().getLeft() + 10.0f, centerY, 4.0f, dot);
  }
}

void Timeline::applyTheme(const Theme& theme) {
  mComplete = theme.controls.accent;
  mCurrent = theme.controls.accent;
  mPending = theme.controls.border;
  mTitle = theme.controls.foreground;
  mDetail = theme.textMuted;
  updateColors();
  setNeedsPaint();
}

void Timeline::rebuild() {
  for (const auto& row: mRows) {
    if (row.text != nullptr) removeChild(row.text);
    if (row.detail != nullptr) removeChild(row.detail);
    if (row.time != nullptr) removeChild(row.time);
  }
  mRows.clear();
  for (const auto& entry: mEntries) {
    Row row;
    row.text = std::make_shared<TextLayer>(entry.title, mFont, mTitle);
    row.detail = std::make_shared<TextLayer>(
      entry.detail.empty() ? entry.time : entry.detail, mFont, mDetail);
    addChild(row.text);
    addChild(row.detail);
    mRows.push_back(std::move(row));
  }
  updateColors();
  setNeedsLayout();
  setNeedsPaint();
}

void Timeline::updateColors() {
  for (size_t index = 0; index < mRows.size(); ++index) {
    const auto state = mEntries[index].state;
    const Color color = state == TimelineState::Complete ? mComplete
      : (state == TimelineState::Current ? mCurrent : mTitle);
    if (mRows[index].text != nullptr) mRows[index].text->setColor(color);
  }
}

} // namespace coral::ui
