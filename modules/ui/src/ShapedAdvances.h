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

#ifndef CORAL_UI_SHAPEDADVANCES_H
#define CORAL_UI_SHAPEDADVANCES_H

#include <graphics/Font.h>
#include <graphics/FontFallback.h>
#include <graphics/TextBlob.h>

#include <cstddef>
#include <string_view>
#include <span>
#include <vector>

namespace coral::ui::detail {

/**
 * Prefix advances of a shaped run, indexed by UTF-8 byte offset.
 *
 * Wrapping and hit testing need "how wide is text[begin..end)?" The honest answer comes from the
 * same shaping that drawing uses: summing per-code-point `measure` calls ignores kerning and
 * ligatures, so a line can break early or overflow by a fraction of a glyph. This table is built
 * from one shaping pass and reports the shaped advance of any byte range.
 *
 * Glyph cluster indices are byte offsets into the shaped text. A cluster's advance is credited at
 * the byte where the next cluster starts, so `advance(a, b)` is exact when `a` and `b` are cluster
 * boundaries and never over-reports inside a cluster. Advances come back from HarfBuzz in font em
 * units and are converted with the owning font's size, exactly like `Font::measureWidth`.
 */
class ShapedAdvances {
 public:
  ShapedAdvances() = default;

  /** Shapes @p text with @p font once. Invalid when the font has no usable metrics. */
  [[nodiscard]] static ShapedAdvances fromFont(const graphics::Font& font, std::string_view text,
    graphics::Font::Direction direction = graphics::Font::Direction::Auto,
    std::span<const graphics::FontFeature> features = {});

  /**
   * Shapes @p text through a fallback chain: each run is shaped with the face that covers it and the
   * run advances are concatenated in logical order. Only the primary face is used when @p fallback
   * is null.
   */
  [[nodiscard]] static ShapedAdvances fromFallback(const graphics::FontFallback& fallback,
    std::string_view text);

  [[nodiscard]] bool valid() const noexcept {
    return mValid;
  }

  /** @return the shaped advance of `[begin, end)`; 0 when out of range. */
  [[nodiscard]] float advance(std::size_t begin, std::size_t end) const noexcept;

  /** @return the shaped advance of the whole text. */
  [[nodiscard]] float width() const noexcept {
    return mPrefix.empty() ? 0.0f : mPrefix.back();
  }

 private:
  void appendRun(std::string_view runText, std::size_t runOffset,
    const graphics::TextBlob& blob, float scale);

  std::vector<float> mPrefix;
  bool mValid = false;
};

} // namespace coral::ui::detail

#endif // CORAL_UI_SHAPEDADVANCES_H
