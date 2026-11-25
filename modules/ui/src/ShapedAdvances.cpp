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

#include "ShapedAdvances.h"

#include <algorithm>
#include <utility>

namespace coral::ui::detail {

namespace {

[[nodiscard]] float emToPixels(const graphics::Font& font) {
  const auto typeface = font.typeface();
  if (typeface == nullptr) return 0.0f;
  const float unitsPerEm = static_cast<float>(typeface->unitsPerEm());
  return unitsPerEm > 0.0f ? font.size() / unitsPerEm : 0.0f;
}

} // namespace

ShapedAdvances ShapedAdvances::fromFont(const graphics::Font& font, const std::string_view text,
    const graphics::Font::Direction direction,
    const std::span<const graphics::FontFeature> features) {
  ShapedAdvances result;
  if (text.empty()) {
    result.mPrefix.assign(1, 0.0f);
    result.mValid = true;
    return result;
  }
  const float scale = emToPixels(font);
  if (scale <= 0.0f) return result;
  const graphics::TextBlob blob = font.shape(text, direction, features);
  if (blob.count() == 0) return result;
  result.mPrefix.assign(text.size() + 1, 0.0f);
  result.appendRun(text, 0, blob, scale);
  result.mValid = true;
  return result;
}

ShapedAdvances ShapedAdvances::fromFallback(const graphics::FontFallback& fallback,
    const std::string_view text) {
  ShapedAdvances result;
  if (text.empty()) {
    result.mPrefix.assign(1, 0.0f);
    result.mValid = true;
    return result;
  }
  std::vector<graphics::FontRun> runs = fallback.shape(text);
  if (runs.empty()) return result;
  result.mPrefix.assign(text.size() + 1, 0.0f);
  std::size_t offset = 0;
  for (const graphics::FontRun& run: runs) {
    if (run.blob == nullptr || run.font == nullptr || offset >= text.size()) break;
    const std::size_t length = std::min(run.text.size(), text.size() - offset);
    if (length == 0) continue;
    result.appendRun(text.substr(offset, length), offset, *run.blob, emToPixels(*run.font));
    offset += length;
  }
  if (offset == 0) return result;
  result.mValid = true;
  return result;
}

void ShapedAdvances::appendRun(const std::string_view runText, const std::size_t runOffset,
    const graphics::TextBlob& blob, const float scale) {
  if (runOffset + runText.size() >= mPrefix.size() || scale <= 0.0f) return;

  // One entry per cluster start, in logical order: several glyphs can share a cluster (marks), and
  // HarfBuzz returns glyphs in visual order.
  std::vector<std::pair<std::size_t, float>> clusters;
  clusters.reserve(blob.count());
  for (const graphics::Glyph& glyph: blob.glyphs()) {
    const std::size_t start = std::min<std::size_t>(glyph.cluster, runText.size());
    if (!clusters.empty() && clusters.back().first == start) {
      clusters.back().second += glyph.advance.x;
    } else {
      clusters.emplace_back(start, glyph.advance.x);
    }
  }
  std::stable_sort(clusters.begin(), clusters.end(),
    [](const std::pair<std::size_t, float>& left, const std::pair<std::size_t, float>& right) {
      return left.first < right.first;
    });

  std::vector<float> steps(runText.size() + 1, 0.0f);
  for (std::size_t index = 0; index < clusters.size(); ++index) {
    const std::size_t clusterEnd =
      index + 1 < clusters.size() ? clusters[index + 1].first : runText.size();
    if (clusterEnd < steps.size()) steps[clusterEnd] += clusters[index].second;
  }
  for (std::size_t index = 1; index <= runText.size(); ++index) {
    mPrefix[runOffset + index] = mPrefix[runOffset + index - 1] + steps[index] * scale;
  }
}

float ShapedAdvances::advance(const std::size_t begin, const std::size_t end) const noexcept {
  if (!mValid || mPrefix.size() < 2 || begin + 1 >= mPrefix.size()) return 0.0f;
  const std::size_t last = std::min(end, mPrefix.size() - 1);
  if (last <= begin) return 0.0f;
  return std::max(0.0f, mPrefix[last] - mPrefix[begin]);
}

} // namespace coral::ui::detail
