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

#include <ui/CandidateListView.h>

#include <graphics/Canvas.h>
#include <graphics/Font.h>
#include <graphics/Paint.h>

#include <algorithm>
#include <cmath>

namespace coral::ui {

namespace {
constexpr float kPaddingX = 10.0f;
constexpr float kPaddingY = 6.0f;
constexpr float kRowGap = 3.0f;
} // namespace

CandidateListView::CandidateListView(std::shared_ptr<const graphics::Font> font)
    : mFont(std::move(font)) {}

CandidateListView::~CandidateListView() = default;

void CandidateListView::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  setNeedsPaint();
}

void CandidateListView::setCandidates(std::vector<std::string> candidates, const size_t selected) {
  mCandidates = std::move(candidates);
  mSelected = mCandidates.empty() ? 0 : std::min(selected, mCandidates.size() - 1);
  setNeedsPaint();
}

void CandidateListView::setSelected(const size_t selected) {
  if (mCandidates.empty()) {
    mSelected = 0;
    return;
  }
  const size_t clamped = std::min(selected, mCandidates.size() - 1);
  if (clamped == mSelected) return;
  mSelected = clamped;
  setNeedsPaint();
}

float CandidateListView::rowHeight() const {
  if (mFont == nullptr) return 20.0f;
  const auto& metrics = mFont->metrics();
  return std::max(20.0f, metrics.ascent + metrics.descent + kRowGap);
}

Size CandidateListView::preferredSize() const {
  if (mFont == nullptr || mCandidates.empty()) return {};
  float width = 80.0f;
  for (const std::string& candidate: mCandidates) {
    width = std::max(width, mFont->measureWidth(candidate) + kPaddingX * 2.0f);
  }
  return { width, rowHeight() * static_cast<float>(mCandidates.size()) + kPaddingY * 2.0f };
}

void CandidateListView::drawSelf(RenderContext& context) {
  if (!visible() || mCandidates.empty() || mFont == nullptr) return;
  auto& canvas = context.canvas();
  const Rect box = bounds();

  graphics::Paint background;
  background.color = graphics::Color::MakeRGBA8(32, 36, 44, 238);
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(box, 6.0f), background);

  const auto& metrics = mFont->metrics();
  const float height = rowHeight();
  graphics::Paint selectedPaint;
  selectedPaint.color = graphics::Color::MakeRGBA8(56, 104, 190, 255);
  selectedPaint.style = graphics::Paint::Style::kFill;
  const Rect selectedRect = Rect::MakeLTRB(box.getLeft() + 3.0f,
    box.getTop() + kPaddingY + static_cast<float>(mSelected) * height,
    box.getRight() - 3.0f,
    box.getTop() + kPaddingY + static_cast<float>(mSelected + 1) * height);
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(selectedRect, 4.0f), selectedPaint);

  graphics::Paint text;
  text.color = graphics::Color::MakeRGBA8(235, 239, 245, 255);
  for (size_t index = 0; index < mCandidates.size(); ++index) {
    const float baseline = box.getTop() + kPaddingY + static_cast<float>(index) * height +
      metrics.ascent;
    context.drawText(mCandidates[index], box.getLeft() + kPaddingX, baseline, *mFont, text);
  }
}

} // namespace coral::ui
