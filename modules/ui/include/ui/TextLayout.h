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

#ifndef CORAL_UI_TEXTLayout_H
#define CORAL_UI_TEXTLayout_H

#include <ui/Types.h>

#include <ui/Geometry.h>
#include <ui/LineBreakPolicy.h>
#include <ui/TextBidi.h>


#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace graphics {
class TextBlob;
}

namespace coral::ui {

/** Lazily shaped run shared by every layout that references the same visual run. */
class LazyTextBlob {
 public:
  LazyTextBlob(std::shared_ptr<const Font> font, std::string text,
               Font::Direction direction, bool mirror);
  /** Interns an equivalent lazy run so repeated runs share one holder. */
  [[nodiscard]] static std::shared_ptr<const LazyTextBlob> create(
    std::shared_ptr<const Font> font, std::string text,
    Font::Direction direction, bool mirror);
  [[nodiscard]] std::shared_ptr<const TextBlob> get() const;

 private:
  std::shared_ptr<const Font> mFont;
  std::string mText;
  Font::Direction mDirection = Font::Direction::Auto;
  bool mMirror = false;
  mutable std::mutex mMutex;
  mutable std::shared_ptr<const TextBlob> mBlob;
};

/** A visual run of one paragraph line after UAX #9 L2; byte range is in the original text. */
struct TextVisualRun {
  size_t start = 0;
  size_t end = 0;
  uint8_t level = 0;
};

/**
 * Read-only rope of UTF-8 blocks. Lines view one block directly; equality and
 * full-text operations walk the blocks without requiring a contiguous copy.
 */
class TextSource {
 public:
  TextSource() = default;
  explicit TextSource(std::string text);

  [[nodiscard]] size_t size() const noexcept { return mSize; }
  [[nodiscard]] bool empty() const noexcept { return mSize == 0; }
  [[nodiscard]] float maxWidth() const;
  /** Returns a view only when [offset, offset+length) is inside one block. */
  [[nodiscard]] std::string_view view(size_t offset, size_t length) const;
  [[nodiscard]] size_t blockCount() const noexcept { return mBlocks.size(); }
  [[nodiscard]] std::string_view block(size_t index) const;
  [[nodiscard]] std::shared_ptr<const std::string> blockOwner(size_t index) const;
  [[nodiscard]] bool equals(std::string_view text) const;
  [[nodiscard]] size_t commonPrefix(std::string_view text) const;
  [[nodiscard]] size_t commonSuffix(std::string_view text) const;
  [[nodiscard]] std::string toString() const;

  void append(std::string text);
  void appendSlice(const TextSource& source, size_t offset, size_t length);

 private:
  struct Block {
    std::shared_ptr<const std::string> data;
    size_t begin = 0;
    size_t length = 0;
  };
  std::vector<Block> mBlocks;
  size_t mSize = 0;
};

/** Offset-adjusted view of one visual run. */
struct VisualRunRef {
  size_t start = 0;
  size_t end = 0;
  uint8_t level = 0;
  std::shared_ptr<const TextBlob> shaped;
  std::shared_ptr<const LazyTextBlob> lazyShaped;

  VisualRunRef() = default;
  VisualRunRef(const TextVisualRun& run, std::ptrdiff_t offset,
    std::shared_ptr<const LazyTextBlob> lazy = nullptr);
  [[nodiscard]] operator TextVisualRun() const { return { start, end, level }; }
};

/** One vector allocation owning a line's runs and their lazy holders. */
struct VisualRunEntry {
  TextVisualRun run;
  std::shared_ptr<const LazyTextBlob> lazy;
};

struct VisualRunBlock {
  std::vector<VisualRunEntry> runs;
  /** UAX #9 L2 order as indices into runs; precomputed once per line. */
  std::vector<uint32_t> order;
};

/** Shared, offset-aware read-only list of visual runs. */
class VisualRunList {
 public:
  class const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = TextVisualRun;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = VisualRunRef;

    const_iterator() = default;
    const_iterator(const VisualRunList* list, size_t index) : mList(list), mIndex(index) {}
    reference operator*() const { return (*mList)[mIndex]; }
    const_iterator& operator++() { ++mIndex; return *this; }
    const_iterator operator++(int) { const_iterator copy = *this; ++mIndex; return copy; }
    bool operator==(const const_iterator& other) const {
      return mList == other.mList && mIndex == other.mIndex;
    }
    bool operator!=(const const_iterator& other) const { return !(*this == other); }

   private:
    const VisualRunList* mList = nullptr;
    size_t mIndex = 0;
  };

  VisualRunList() = default;
  explicit VisualRunList(std::shared_ptr<const VisualRunBlock> block, std::ptrdiff_t offset = 0)
      : mBlock(std::move(block)), mOffset(offset) {}

  [[nodiscard]] size_t size() const noexcept {
    return mBlock != nullptr ? mBlock->runs.size() : 0;
  }
  [[nodiscard]] bool empty() const noexcept { return size() == 0; }
  [[nodiscard]] VisualRunRef operator[](size_t index) const;
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }
  [[nodiscard]] const std::shared_ptr<const VisualRunBlock>& block() const { return mBlock; }
  /** @return the precomputed L2 order (empty when no reordering was needed). */
  [[nodiscard]] const std::vector<uint32_t>& order() const {
    static const std::vector<uint32_t> empty;
    return mBlock != nullptr ? mBlock->order : empty;
  }
  [[nodiscard]] std::shared_ptr<const LazyTextBlob> lazy(size_t index) const {
    if (mBlock == nullptr || index >= mBlock->runs.size()) return nullptr;
    return mBlock->runs[index].lazy;
  }
  [[nodiscard]] std::ptrdiff_t offset() const noexcept { return mOffset; }

 private:
  std::shared_ptr<const VisualRunBlock> mBlock;
  std::ptrdiff_t mOffset = 0;
};

/** Horizontal placement of a line inside its wrap width. */
enum class TextAlign : uint8_t {
  Left,
  Center,
  Right,
  /** Distribute slack after spaces on every line except the last of a paragraph. */
  Justify,
};

/** Paragraph direction: Auto follows the first strong character (P2/P3). */
enum class TextDirectionMode : uint8_t {
  Auto,
  Ltr,
  Rtl,
};

/** Writing direction of the text flow. */
enum class TextOrientation : uint8_t {
  Horizontal,
  /** Upright glyphs; columns advance to the right. */
  Vertical,
  /** Upright CJK, rotated Latin; columns advance to the right. */
  VerticalMixed,
  /** Upright CJK, rotated Latin; columns advance to the left (vertical-rl). */
  VerticalRl,
};

/** One wrapped paragraph line and its measured width. */
struct TextLine {
  /** View into TextLayout::source or ownedText; valid while that layout is alive. */
  std::string_view text;
  float width = 0.0f;
  /** Horizontal offset from the wrap edge, set by TextAlign. */
  float xOffset = 0.0f;
  /** Extra advance after every space when TextAlign::Justify is used; 0 otherwise. */
  float justifySpacing = 0.0f;
  /** Byte range in the original UTF-8 text that produced this line (trimmed whitespace excluded). */
  size_t start = 0;
  size_t end = 0;
  /** True when the line ends with a hyphenation/SHY break and needs a hyphen glyph. */
  bool hyphenated = false;
  /** True for a synthesized line (ellipsis) whose paragraph base direction is RTL. */
  bool rtl = false;
  /** Visual runs in logical order; empty for a line that needs no Bidi reordering. */
  VisualRunList visualRuns;
  /** Keeps a synthesized string (ellipsis) alive. */
  std::shared_ptr<const std::string> ownedText;
  /** Keeps a previous TextSource alive for reused lines. */
  std::shared_ptr<const TextSource> ownedSource;
  /** Shared shaped run for a non-Bidi line; built at layout font size. */
  std::shared_ptr<const TextBlob> shaped;
};

/**
 * Offset-adjusted read-only view of a TextLine. LineList segments store a
 * base line and a byte delta; materializing a LineRef applies that delta to
 * start/end and visual-run offsets without copying the backing block.
 */
struct LineRef {
  std::string_view text;
  float width = 0.0f;
  float xOffset = 0.0f;
  float justifySpacing = 0.0f;
  size_t start = 0;
  size_t end = 0;
  bool hyphenated = false;
  bool rtl = false;
  VisualRunList visualRuns;
  std::shared_ptr<const std::string> ownedText;
  std::shared_ptr<const TextSource> ownedSource;
  std::shared_ptr<const TextBlob> shaped;

  LineRef() = default;
  LineRef(const TextLine& line, const std::ptrdiff_t offset);

  [[nodiscard]] operator TextLine() const {
    TextLine line;
    line.text = text;
    line.width = width;
    line.xOffset = xOffset;
    line.justifySpacing = justifySpacing;
    line.start = start;
    line.end = end;
    line.hyphenated = hyphenated;
    line.rtl = rtl;
    line.visualRuns = visualRuns;
    line.ownedText = ownedText;
    line.ownedSource = ownedSource;
    line.shaped = shaped;
    return line;
  }
};

/** @return visual run indices for @p line, applying UAX #9 L2. */
[[nodiscard]] std::vector<size_t> visualRunOrder(const TextLine& line);

/** @return visual run indices for an arbitrary logical run list, applying UAX #9 L2. */
[[nodiscard]] std::vector<size_t> visualRunOrder(const VisualRunList& runs);
[[nodiscard]] std::vector<size_t> visualRunOrder(const std::vector<TextVisualRun>& runs);

/** Optional truncation policy for a layout. */
struct TextLayoutOptions {
  float maxWidth = 0.0f;
  /** 0 means unlimited; a positive value clamps the line count. */
  int maxLines = 0;
  /** Append a single-line ellipsis to the last line when maxLines truncates the text. */
  bool ellipsis = false;
  /** Optional CSS-like tailoring; the default policy preserves UAX #14 exactly. */
  LineBreakPolicy policy;
  /** Horizontal placement inside maxWidth; no effect when maxWidth is 0. */
  TextAlign align = TextAlign::Left;
  /** Line box height in logical units; 0 uses the font's ascent+descent+lineGap. */
  float lineHeight = 0.0f;
  /** Vertical lays out columns; maxWidth is then the maximum column length. */
  TextOrientation orientation = TextOrientation::Horizontal;
  /** Paragraph direction; Auto is the CSS-like default (P2/P3). */
  TextDirectionMode direction = TextDirectionMode::Auto;
  /** CSS unicode-bidi spans; applied after UAX #9, may nest. */
  std::vector<BidiSpan> bidiSpans;
  /** A tab advances this many space widths; 0 keeps the U+0009 code point unchanged. */
  uint32_t tabSize = 8;
};

/** A line sequence that can share read-only ranges from an older layout. */
class LineList {
 public:
  class const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = TextLine;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = LineRef;

    const_iterator() = default;
    const_iterator(const LineList* list, const size_t index) : mList(list), mIndex(index) {}
    reference operator*() const { return (*mList)[mIndex]; }
    const_iterator& operator++() { ++mIndex; return *this; }
    const_iterator operator++(int) { const_iterator copy = *this; ++mIndex; return copy; }
    bool operator==(const const_iterator& other) const { return mList == other.mList && mIndex == other.mIndex; }
    bool operator!=(const const_iterator& other) const { return !(*this == other); }

   private:
    const LineList* mList = nullptr;
    size_t mIndex = 0;
  };

  [[nodiscard]] size_t size() const noexcept { return mSize; }
  [[nodiscard]] bool empty() const noexcept { return mSize == 0; }
  [[nodiscard]] float maxWidth() const;
  [[nodiscard]] LineRef operator[](size_t index) const;
  TextLine& operator[](size_t index);
  [[nodiscard]] LineRef front() const { return (*this)[0]; }
  [[nodiscard]] LineRef back() const { return (*this)[mSize - 1]; }
  TextLine& back() { return (*this)[mSize - 1]; }

  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, mSize); }
  std::vector<TextLine>::iterator begin();
  std::vector<TextLine>::iterator end();

  void reserve(size_t capacity);
  void push_back(TextLine line);
  void pop_back();
  void resize(size_t size);
  void clear() noexcept;

  /** Appends [begin, end) from @p source without copying its line block. */
  void appendShared(const LineList& source, size_t begin, size_t end);
  /** Appends [begin, end) with a byte delta applied lazily by LineRef. */
  void appendSharedShifted(const LineList& source, size_t begin, size_t end, std::ptrdiff_t offset);
  /** Moves the current build into a read-only segment so shared ranges can follow it. */
  void seal();

 private:
  struct Segment {
    std::shared_ptr<const std::vector<TextLine>> data;
    size_t begin = 0;
    size_t end = 0;
    std::ptrdiff_t offset = 0;
  };
  [[nodiscard]] LineRef refer(size_t index) const;
  void materialize();

  std::vector<Segment> mSegments;
  std::shared_ptr<std::vector<TextLine>> mBuild;
  size_t mSize = 0;
};

/**
 * A wrapped run of text.
 *
 * Breaking follows the generated Unicode 18.0.0 UAX #14 DFA; line construction is
 * greedy and keeps the last `AllowBreak` that fits. `MandatoryBreak` (CR/LF/NL/BK)
 * starts a new line and the separator is not part of the rendered line. Rich runs
 * and per-run shaping stay in RichText; this interface -- lines and a measured
 * size -- is what the layers consume either way.
 */
struct TextLayout {
  LineList lines;
  /** Rope owning the laid-out UTF-8 bytes; reused lines may share an older source via borrowedSources/ownedSource. */
  std::shared_ptr<const TextSource> source;
  /** Sources kept alive for line blocks shared from previous layouts. */
  std::vector<std::shared_ptr<const TextSource>> borrowedSources;
  Size size{};
  /** Line box height used for size.height and by the drawing layers; 0 means font metrics. */
  float lineHeight = 0.0f;
  /** Alignment used for each line's xOffset; kept so incremental reflow can compare options. */
  TextAlign align = TextAlign::Left;
  /** Horizontal or upright-vertical flow. */
  TextOrientation orientation = TextOrientation::Horizontal;
  /** Effective paragraph direction after applying TextLayoutOptions::direction. */
  TextDirection baseDirection = TextDirection::Ltr;
  /** Requested direction mode (Auto/Ltr/Rtl). */
  TextDirectionMode directionMode = TextDirectionMode::Auto;
  /** Policy used to produce this layout; incremental reflow rejects a changed policy. */
  std::shared_ptr<const LineBreakPolicy> policy;
  /** True when maxLines dropped content (whether or not an ellipsis was appended). */
  bool truncated = false;

  [[nodiscard]] bool empty() const noexcept { return lines.empty(); }
};

/** Process-wide hit/miss counters for the Bidi line visual-run cache. */
struct BidiLineCacheStats {
  uint64_t hits = 0;
  uint64_t misses = 0;
  uint64_t stores = 0;
  uint64_t entries = 0;
};

[[nodiscard]] BidiLineCacheStats bidiLineCacheStats();
void resetBidiLineCacheStats();

/** Lays @p text out at @p font, wrapping at @p maxWidth (0 or less means no wrapping). */
[[nodiscard]] TextLayout layoutText(const std::string& text, const Font& font, float maxWidth);

/** Line-break/truncation options; the default is the legacy wrap-at-width behaviour. */
[[nodiscard]] TextLayout layoutText(const std::string& text, const Font& font,
        const TextLayoutOptions& options);

/** Fallback-aware layout: measures each run in the face that actually covers its characters. */
[[nodiscard]] TextLayout layoutText(const std::string& text, const Font& font,
        const FontFallback& fallback, const TextLayoutOptions& options);

/** Byte range of one edit: [start, oldEnd) replaced by [start, newEnd). */
struct TextEditRange {
  size_t start = 0;
  size_t oldEnd = 0;
  size_t newEnd = 0;
};

/**
 * Incrementally reflows @p previous for @p newText.
 *
 * Lines entirely before the first changed byte are reused; layout resumes at that line's
 * start and runs to the end of the text. Falls back to a full layout when the edit touches
 * Bidi text, truncation/ellipsis, or a layout without an owned source.
 */
[[nodiscard]] TextLayout updateTextLayout(const TextLayout& previous, const std::string_view newText,
        const Font& font, const TextLayoutOptions& options);

[[nodiscard]] TextLayout updateTextLayout(const TextLayout& previous, const std::string_view newText,
        const TextEditRange& edit, const Font& font, const TextLayoutOptions& options);

/** Moves reusable prefix/suffix lines out of @p previous instead of copying them. */
[[nodiscard]] TextLayout updateTextLayout(TextLayout&& previous, const std::string_view newText,
        const Font& font, const TextLayoutOptions& options);

[[nodiscard]] TextLayout updateTextLayout(TextLayout&& previous, const std::string_view newText,
        const TextEditRange& edit, const Font& font, const TextLayoutOptions& options);

} // namespace coral::ui

#endif // CORAL_UI_TEXTLayout_H
