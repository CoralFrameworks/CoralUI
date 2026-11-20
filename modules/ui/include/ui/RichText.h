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

#ifndef CORAL_UI_RICHTEXT_H
#define CORAL_UI_RICHTEXT_H

#include <ui/Types.h>

#include <ui/Geometry.h>
#include <ui/LineBreakPolicy.h>
#include <ui/TextLayout.h>

#include <geometry/Color.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace graphics {
}

namespace coral::ui {

/** A run of text sharing one font, colour and decoration. */
struct RichTextRun {
  std::string text;
  std::shared_ptr<const Font> font;
  /** Optional per-code-point fallback faces for this run. */
  std::shared_ptr<const FontFallback> fallback;
  Color color = Color::White();
  bool underline = false;
};

struct RichTextSegment {
  std::string text;
  float width = 0.0f;
  std::shared_ptr<const Font> font;
  Color color{};
  bool underline = false;
  /** Byte range in the concatenated runs. */
  size_t start = 0;
  size_t end = 0;
};

/**
 * Copy-on-write segment storage. Layout builds a vector, then `seal()` shares it
 * between cached/incrementally reused lines so copying a RichTextLine is cheap.
 */
class RichTextSegments {
 public:
  using Container = std::vector<RichTextSegment>;
  using const_iterator = Container::const_iterator;
  using iterator = Container::iterator;

  RichTextSegments() = default;

  [[nodiscard]] size_t size() const noexcept { return vector().size(); }
  [[nodiscard]] bool empty() const noexcept { return vector().empty(); }
  [[nodiscard]] const RichTextSegment& operator[](size_t index) const { return vector()[index]; }
  [[nodiscard]] RichTextSegment& operator[](size_t index) { return mutableVector()[index]; }
  [[nodiscard]] const RichTextSegment& front() const { return vector().front(); }
  [[nodiscard]] RichTextSegment& front() { return mutableVector().front(); }
  [[nodiscard]] const RichTextSegment& back() const { return vector().back(); }
  [[nodiscard]] RichTextSegment& back() { return mutableVector().back(); }
  [[nodiscard]] const RichTextSegment* data() const noexcept { return vector().data(); }
  [[nodiscard]] const_iterator begin() const { return vector().begin(); }
  [[nodiscard]] const_iterator end() const { return vector().end(); }
  [[nodiscard]] iterator begin() { return mutableVector().begin(); }
  [[nodiscard]] iterator end() { return mutableVector().end(); }

  void push_back(RichTextSegment segment) { mutableVector().push_back(std::move(segment)); }
  void resize(size_t size) { mutableVector().resize(size); }
  void clear() {
    mShared.reset();
    mOwned = std::make_shared<Container>();
  }
  [[nodiscard]] const Container& vector() const {
    static const Container empty;
    if (mOwned != nullptr) return *mOwned;
    return mShared != nullptr ? *mShared : empty;
  }
  /** Freezes the current vector so copies share it. */
  void seal() {
    if (mOwned != nullptr) {
      mShared = mOwned;
      mOwned.reset();
    }
  }

 private:
  [[nodiscard]] Container& mutableVector() {
    if (mOwned == nullptr) {
      mOwned = mShared != nullptr
        ? std::make_shared<Container>(*mShared)
        : std::make_shared<Container>();
    } else if (mOwned.use_count() > 1) {
      mOwned = std::make_shared<Container>(*mOwned);
    }
    return *mOwned;
  }

  std::shared_ptr<const Container> mShared;
  std::shared_ptr<Container> mOwned;
};

struct RichTextLine {
  RichTextSegments segments;
  float width = 0.0f;
  /** Horizontal offset from the wrap edge, set by TextAlign. */
  float xOffset = 0.0f;
  /** Extra advance after every space when TextAlign::Justify is used; 0 otherwise. */
  float justifySpacing = 0.0f;
  float height = 0.0f;
  size_t start = 0;
  size_t end = 0;
  /** True when the line ends with a hyphenation/SHY break. */
  bool hyphenated = false;
  /** True when the line is a synthetic maxLines/ellipsis line. */
  bool ellipsis = false;
  /** Paragraph base direction resolved by UAX #9. */
  bool rtl = false;
  /** Visual runs in logical order; empty when the line needs no Bidi reordering. */
  std::vector<TextVisualRun> visualRuns;
  /** UAX #9 L2 order as indices into visualRuns; precomputed once per line. */
  std::vector<uint32_t> visualOrder;
};

/** Layout options for rich text: width, line box height, alignment and policy. */
struct RichTextLayoutOptions {
  float maxWidth = 0.0f;
  /** Fixed line box height; 0 uses the largest font metrics on each line. */
  float lineHeight = 0.0f;
  TextAlign align = TextAlign::Left;
  LineBreakPolicy policy;
  /** 0 means unlimited; a positive value clamps the line count. */
  int maxLines = 0;
  /** Append an ellipsis to the last line when maxLines truncates the text. */
  bool ellipsis = false;
  /** Horizontal or upright-vertical column flow. */
  TextOrientation orientation = TextOrientation::Horizontal;
  /** Paragraph direction; Auto still runs the UBA only when needed. */
  TextDirectionMode direction = TextDirectionMode::Auto;
  /** CSS unicode-bidi spans; applied after UAX #9, may nest. */
  std::vector<BidiSpan> bidiSpans;
  /** A tab advances this many space widths; 0 keeps the U+0009 code point unchanged. */
  uint32_t tabSize = 8;
};

/** Shared, read-only view of the lines in a rich-text layout. */
class RichTextLineList {
 public:
  using Container = std::vector<RichTextLine>;

  class const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = RichTextLine;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = const RichTextLine&;

    const_iterator() = default;
    const_iterator(const RichTextLineList* list, size_t index) : mList(list), mIndex(index) {}
    reference operator*() const { return (*mList)[mIndex]; }
    const_iterator& operator++() { ++mIndex; return *this; }
    const_iterator operator++(int) { const_iterator copy = *this; ++mIndex; return copy; }
    bool operator==(const const_iterator& other) const {
      return mList == other.mList && mIndex == other.mIndex;
    }
    bool operator!=(const const_iterator& other) const { return !(*this == other); }

   private:
    const RichTextLineList* mList = nullptr;
    size_t mIndex = 0;
  };

  RichTextLineList() = default;
  explicit RichTextLineList(std::shared_ptr<const Container> lines) {
    appendContainer(std::move(lines));
  }

  /** Appends all lines of @p other without copying them. */
  void appendRange(const RichTextLineList& other) {
    for (const Segment& segment: other.mSegments) {
      mSegments.push_back(segment);
      mSize += segment.end - segment.begin;
    }
    mMaterialized.reset();
  }
  /** Appends [@p begin, @p end) of @p other, sharing whole or partial segments. */
  void appendSubrange(const RichTextLineList& other, size_t begin, size_t end) {
    if (begin >= end) return;
    size_t global = 0;
    for (const Segment& segment: other.mSegments) {
      const size_t count = segment.end - segment.begin;
      const size_t localBegin = begin > global ? std::min(begin - global, count) : 0;
      const size_t localEnd = end > global ? std::min(end - global, count) : 0;
      if (localBegin < localEnd) {
        Segment slice;
        slice.data = segment.data;
        slice.begin = segment.begin + localBegin;
        slice.end = segment.begin + localEnd;
        mSize += slice.end - slice.begin;
        mSegments.push_back(std::move(slice));
      }
      global += count;
      if (global >= end) break;
    }
    mMaterialized.reset();
  }
  /** Number of shared segments; 1 for a freshly materialized block. */
  [[nodiscard]] size_t segmentCount() const noexcept { return mSegments.size(); }
  /** Appends one shared block without copying its lines. */
  void appendContainer(std::shared_ptr<const Container> lines) {
    if (lines == nullptr || lines->empty()) return;
    Segment segment;
    segment.data = std::move(lines);
    segment.end = segment.data->size();
    mSize += segment.end;
    mSegments.push_back(std::move(segment));
    mMaterialized.reset();
  }

  [[nodiscard]] size_t size() const noexcept { return mSize; }
  [[nodiscard]] bool empty() const noexcept { return mSize == 0; }
  [[nodiscard]] const RichTextLine& operator[](size_t index) const {
    for (const Segment& segment: mSegments) {
      const size_t count = segment.end - segment.begin;
      if (index < count) return (*segment.data)[segment.begin + index];
      index -= count;
    }
    return emptyLine();
  }
  [[nodiscard]] const RichTextLine& front() const { return (*this)[0]; }
  [[nodiscard]] const RichTextLine& back() const { return (*this)[mSize - 1]; }
  [[nodiscard]] const_iterator begin() const { return const_iterator(this, 0); }
  [[nodiscard]] const_iterator end() const { return const_iterator(this, mSize); }
  /** @return a contiguous view; materializes a copy when the list is segmented. */
  [[nodiscard]] const Container& vector() const {
    if (mSegments.empty()) return emptyContainer();
    if (mSegments.size() == 1 && mSegments.front().begin == 0) {
      return *mSegments.front().data;
    }
    if (mMaterialized == nullptr) {
      mMaterialized = std::make_shared<Container>();
      mMaterialized->reserve(mSize);
      for (const RichTextLine& line: *this) mMaterialized->push_back(line);
    }
    return *mMaterialized;
  }

 private:
  struct Segment {
    std::shared_ptr<const Container> data;
    size_t begin = 0;
    size_t end = 0;
  };
  static const RichTextLine& emptyLine() {
    static const RichTextLine empty;
    return empty;
  }
  static const Container& emptyContainer() {
    static const Container empty;
    return empty;
  }
  std::vector<Segment> mSegments;
  size_t mSize = 0;
  mutable std::shared_ptr<Container> mMaterialized;
};

/**
 * Wraps styled runs into lines, using the same Unicode UAX #14 boundaries as
 * TextLayout and breaking overlong words at codepoints only as a last resort.
 */
struct RichTextLayout {
  RichTextLineList lines;
  Size size{};
  /** Fixed line box height; 0 when lines size themselves from their fonts. */
  float lineHeight = 0.0f;
  TextAlign align = TextAlign::Left;
  /** True when maxLines dropped content (whether or not an ellipsis was appended). */
  bool truncated = false;
  /** Plain text used by incremental reflow; shared with cached layouts. */
  std::shared_ptr<const std::string> text;
  /** Wrap width the layout was produced with (0 = no wrapping). */
  float maxWidth = 0.0f;
  /** Horizontal or vertical column flow. */
  TextOrientation orientation = TextOrientation::Horizontal;
  /** Effective paragraph direction used to build the visual runs. */
  TextDirectionMode direction = TextDirectionMode::Auto;

  [[nodiscard]] bool empty() const noexcept { return lines.empty(); }
};

/**
 * Wraps styled runs into lines, using the same Unicode UAX #14 boundaries as
 * TextLayout and breaking overlong words at codepoints only as a last resort.
 */
[[nodiscard]] RichTextLayout layoutRichText(const std::vector<RichTextRun>& runs, float maxWidth);

/** Policy-aware variant; non-default policies are cached by their key. */
[[nodiscard]] RichTextLayout layoutRichText(const std::vector<RichTextRun>& runs, float maxWidth,
  const LineBreakPolicy& policy);

/** Full options variant: wrap width, line height and alignment. */
[[nodiscard]] RichTextLayout layoutRichText(const std::vector<RichTextRun>& runs,
  const RichTextLayoutOptions& options);

/**
 * Incrementally reflows @p runs after a text edit, reusing the line ranges before
 * and after the changed region from @p previous.
 *
 * Falls back to a full layout when the options changed incompatibly, when
 * maxLines/ellipsis or Justify is in use, or when nothing can be reused. The
 * optional @p edit hint is accepted for API symmetry; prefix/suffix discovery
 * does not require it.
 */
[[nodiscard]] RichTextLayout updateRichTextLayout(const RichTextLayout& previous,
  const std::vector<RichTextRun>& runs, const RichTextLayoutOptions& options,
  const TextEditRange* edit = nullptr);

} // namespace coral::ui

#endif // CORAL_UI_RICHTEXT_H
