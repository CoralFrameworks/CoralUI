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

#include <ui/Flex.h>
#include <cmath>

#include <algorithm>
#include <vector>

namespace coral::ui {

Flex::Flex(const Axis axis) : mAxis(axis) {}

void Flex::setAxis(const Axis axis) {
  if (mAxis == axis) {
    return;
  }
  mAxis = axis;
  setNeedsLayout();
}

void Flex::setPadding(const Insets& padding) {
  if (mPadding == padding) return;
  mPadding = padding;
  setNeedsLayout();
}

void Flex::setGap(const float gap) {
  if (mGap == gap) {
    return;
  }
  mGap = std::max(0.0f, gap);
  setNeedsLayout();
}

void Flex::setCrossAlign(const Align align) {
  if (mCrossAlign == align) {
    return;
  }
  mCrossAlign = align;
  setNeedsLayout();
}

void Flex::setJustify(const Justify justify) {
  if (mJustify == justify) {
    return;
  }
  mJustify = justify;
  setNeedsLayout();
}

void Flex::setWrap(const bool wrap) {
  if (mWrap == wrap) {
    return;
  }
  mWrap = wrap;
  setNeedsLayout();
}

void Flex::setLineGap(const float gap) {
  const float next = std::max(0.0f, gap);
  if (mLineGap == next) {
    return;
  }
  mLineGap = next;
  setNeedsLayout();
}

namespace {

struct FlexItem {
  std::shared_ptr<Layer> layer;
  Insets margin;
  float main = 0.0f;
  float cross = 0.0f;
  float grow = 0.0f;
  float shrink = 1.0f;
  float minMain = 0.0f;
  float minCross = 0.0f;
  float maxMain = 0.0f;
  float maxCross = 0.0f;
  AlignSelf alignSelf = AlignSelf::Auto;
};

float mainOf(const Size& size, const Axis axis) {
  return axis == Axis::Row ? size.width : size.height;
}

float crossOf(const Size& size, const Axis axis) {
  return axis == Axis::Row ? size.height : size.width;
}

Size makeSize(const Axis axis, const float main, const float cross) {
  return axis == Axis::Row ? Size{ main, cross } : Size{ cross, main };
}

float clamp(float value, const float minimum, const float maximum) {
  float result = std::max(value, minimum);
  if (maximum > 0.0f) result = std::min(result, maximum);
  return result;
}

/**
 * The child's intrinsic size, but only when a hint actually asks for it.
 *
 * `resolveItemSize` reads `intrinsic` only for `fitContent*`, so evaluating `intrinsicSize()` at the
 * call site made every measure *and* every layout pass shape every text child: a 1480-node page paid
 * ~27 us per node for a value almost no child uses.
 */
Size intrinsicIfNeeded(const Layer& child, const LayoutParams& params) {
  if (!params.fitContentMain && !params.fitContentCross) return Size{ -1.0f, -1.0f };
  return child.intrinsicSize();
}

/** The item's main/cross extent from its measured size plus the layout hints. */
void resolveItemSize(const LayoutParams& params, const Axis axis, const float innerMain,
    const float innerCross, const Size& measured, float* main, float* cross,
    const Size& intrinsic = { -1.0f, -1.0f }) {
  *main = mainOf(measured, axis);
  *cross = crossOf(measured, axis);
  if (params.basis >= 0.0f) {
    *main = params.basis;
  }
  if (axis == Axis::Row) {
    if (params.widthPercent >= 0.0f) *main = innerMain * params.widthPercent;
    if (params.heightPercent >= 0.0f) *cross = innerCross * params.heightPercent;
  } else {
    if (params.heightPercent >= 0.0f) *main = innerMain * params.heightPercent;
    if (params.widthPercent >= 0.0f) *cross = innerCross * params.widthPercent;
  }
  if (params.aspectRatio > 0.0f) {
    // ratio is width / height: with the main axis resolved, the cross axis follows the ratio.
    *cross = axis == Axis::Row ? *main / params.aspectRatio : *main * params.aspectRatio;
  }
  // "fit the content": the most specific rule, so it runs after basis/percent/ratio.
  if (params.fitContentMain) {
    const float value = axis == Axis::Row ? intrinsic.width : intrinsic.height;
    if (value >= 0.0f) *main = value;
  }
  if (params.fitContentCross) {
    const float value = axis == Axis::Row ? intrinsic.height : intrinsic.width;
    if (value >= 0.0f) *cross = value;
  }
  const float minMain = axis == Axis::Row ? params.minWidth : params.minHeight;
  const float minCross = axis == Axis::Row ? params.minHeight : params.minWidth;
  const float maxMain = axis == Axis::Row ? params.maxWidth : params.maxHeight;
  const float maxCross = axis == Axis::Row ? params.maxHeight : params.maxWidth;
  *main = clamp(std::max(0.0f, *main), minMain, maxMain);
  *cross = clamp(std::max(0.0f, *cross), minCross, maxCross);
}

Align resolveAlign(const AlignSelf self, const Align container) {
  switch (self) {
    case AlignSelf::Start: return Align::Start;
    case AlignSelf::Center: return Align::Center;
    case AlignSelf::End: return Align::End;
    case AlignSelf::Stretch: return Align::Stretch;
    case AlignSelf::Baseline: return Align::Baseline;
    case AlignSelf::Auto: break;
  }
  return container;
}

/**
 * The rectangle a child may occupy, in the child's own (width, height) axes.
 *
 * `innerMain`/`innerCross` are the container's extents along its main/cross axis. For a Column the
 * cross extent is the width, so passing `innerMain` as the child's width would hand a text child the
 * container's height as its wrap width.
 */
Size childMeasureSpace(const Axis axis, const float innerMain, const float innerCross,
    const LayoutParams& params) {
  const float width = axis == Axis::Row ? innerMain : innerCross;
  const float height = axis == Axis::Row ? innerCross : innerMain;
  return Size{
    std::max(0.0f, width - params.margin.horizontal()),
    std::max(0.0f, height - params.margin.vertical())
  };
}

/** Lays one absolute child out against the parent's inner rectangle. */
void layoutAbsolute(Layer& child, const Rect& inner) {
  const LayoutParams& params = child.layoutParams();
  const Size measured = child.measuredSize();
  float width = params.widthPercent >= 0.0f ? inner.getWidth() * params.widthPercent
                                            : measured.width;
  float height = params.heightPercent >= 0.0f ? inner.getHeight() * params.heightPercent
                                              : measured.height;
  if (params.hasLeft && params.hasRight && params.widthPercent < 0.0f) {
    width = inner.getWidth() - params.left - params.right;
  }
  if (params.hasTop && params.hasBottom && params.heightPercent < 0.0f) {
    height = inner.getHeight() - params.top - params.bottom;
  }
  width = clamp(std::max(0.0f, width), params.minWidth, params.maxWidth);
  height = clamp(std::max(0.0f, height), params.minHeight, params.maxHeight);

  float x = inner.getLeft();
  if (params.hasLeft) {
    x = inner.getLeft() + params.left;
  } else if (params.hasRight) {
    x = inner.getRight() - params.right - width;
  }
  float y = inner.getTop();
  if (params.hasTop) {
    y = inner.getTop() + params.top;
  } else if (params.hasBottom) {
    y = inner.getBottom() - params.bottom - height;
  }
  // An absolute child whose width and height both came from insets/percentages has been sized by the
  // parent, exactly like the flow path above; one that falls back to its measurement has not.
  const bool widthAssigned = params.widthPercent >= 0.0f ||
    (params.hasLeft && params.hasRight);
  const bool heightAssigned = params.heightPercent >= 0.0f ||
    (params.hasTop && params.hasBottom);
  child.noteParentAssignedSize(widthAssigned && heightAssigned);
  child.setBounds(Rect::MakeXYWH(x, y, width, height));
  child.layout();
}

} // namespace anonymous

Size Flex::measure(const Size& available) {
  const float availableMain = (mAxis == Axis::Row ? available.width : available.height) -
                              (mAxis == Axis::Row ? mPadding.horizontal() : mPadding.vertical());
  const float availableCross = (mAxis == Axis::Row ? available.height : available.width) -
                               (mAxis == Axis::Row ? mPadding.vertical() : mPadding.horizontal());
  const float innerMain = std::max(0.0f, availableMain);
  const float innerCross = std::max(0.0f, availableCross);
  const Size innerSize{ mAxis == Axis::Row ? innerMain : innerCross,
    mAxis == Axis::Row ? innerCross : innerMain };

  float totalMain = 0.0f;
  float maxCross = 0.0f;
  // Baseline-aligned items need max(ascent) + max(descent), which can exceed the tallest box.
  float maxBaseline = 0.0f;
  float maxDescender = 0.0f;
  uint32_t count = 0;
  float lineMain = 0.0f;
  float lineCross = 0.0f;
  float lineBaseline = 0.0f;
  float lineDescender = 0.0f;
  float maxLineMain = 0.0f;
  float totalCross = 0.0f;
  uint32_t lines = 0;

  for (const std::shared_ptr<Layer>& child: children()) {
    if (!child->visible() || child->floating()) {
      continue;
    }
    const LayoutParams& params = child->layoutParams();
    Size childAvailable = childMeasureSpace(mAxis, innerMain, innerCross, params);
    // Unbounded measurement: 0 tells widgets such as text "no width constraint, report your
    // natural size"; margins still count against the other axis.
    if (mAxis == Axis::Row) {
      if (params.unboundedMain) childAvailable.width = 0.0f;
      if (params.unboundedCross) childAvailable.height = 0.0f;
    } else {
      if (params.unboundedMain) childAvailable.height = 0.0f;
      if (params.unboundedCross) childAvailable.width = 0.0f;
    }
    const Size childSize = child->measureCached(childAvailable);
    if (params.position == Position::Absolute) {
      continue;
    }

    float childMain = 0.0f;
    float childCross = 0.0f;
    resolveItemSize(params, mAxis, innerMain, innerCross, childSize, &childMain, &childCross,
      intrinsicIfNeeded(*child, params));
    const float marginMain = mAxis == Axis::Row ? params.margin.horizontal() : params.margin.vertical();
    const float marginCross = mAxis == Axis::Row ? params.margin.vertical() : params.margin.horizontal();
    ++count;

    if (mWrap) {
      const float addition = childMain + marginMain + (lineMain > 0.0f ? mGap : 0.0f);
      if (lineMain > 0.0f && lineMain + addition > innerMain) {
        maxLineMain = std::max(maxLineMain, lineMain);
        totalCross += std::max(lineCross, lineBaseline + lineDescender);
        ++lines;
        lineMain = 0.0f;
        lineCross = 0.0f;
        lineBaseline = 0.0f;
        lineDescender = 0.0f;
      }
      lineMain += childMain + marginMain + (lineMain > 0.0f ? mGap : 0.0f);
      lineCross = std::max(lineCross, childCross + marginCross);
      if (resolveAlign(params.alignSelf, mCrossAlign) == Align::Baseline) {
        const float baseline = child->baselineOffset();
        if (baseline >= 0.0f) {
          lineBaseline = std::max(lineBaseline, baseline);
          lineDescender = std::max(lineDescender, std::max(0.0f, childCross - baseline));
        }
      }
    } else {
      totalMain += childMain + marginMain;
      maxCross = std::max(maxCross, childCross + marginCross);
      if (resolveAlign(params.alignSelf, mCrossAlign) == Align::Baseline) {
        const float baseline = child->baselineOffset();
        if (baseline >= 0.0f) {
          maxBaseline = std::max(maxBaseline, baseline);
          maxDescender = std::max(maxDescender, std::max(0.0f, childCross - baseline));
        }
      }
    }
  }

  if (mWrap) {
    if (lineMain > 0.0f || count > 0) {
      maxLineMain = std::max(maxLineMain, lineMain);
      totalCross += std::max(lineCross, lineBaseline + lineDescender);
      ++lines;
    }
    if (lines > 1) {
      totalCross += (mLineGap > 0.0f ? mLineGap : mGap) * static_cast<float>(lines - 1);
    }
    totalMain = maxLineMain;
  } else {
    if (count > 1) {
      totalMain += mGap * static_cast<float>(count - 1);
    }
    totalCross = maxCross;
    if (maxBaseline > 0.0f || maxDescender > 0.0f) {
      totalCross = std::max(totalCross, maxBaseline + maxDescender);
    }
  }

  const Size size{ mAxis == Axis::Row ? totalMain + mPadding.horizontal()
                                      : totalCross + mPadding.horizontal(),
    mAxis == Axis::Row ? totalCross + mPadding.vertical() : totalMain + mPadding.vertical() };
  (void) innerSize;
  setMeasuredSize(size);
  return size;
}

Size Flex::measureWithConstraints(const Constraints& constraints) {
  // The legacy measure clamps to the available size itself; the addition here is the maximum, which
  // matters when a minimum inside the flex pushes the result past what the parent offered.
  Size measured = measure(constraints.availableSize());
  measured.width = std::clamp(measured.width, constraints.minWidth, constraints.maxWidth);
  measured.height = std::clamp(measured.height, constraints.minHeight, constraints.maxHeight);
  return measured;
}

void Flex::layout() {
  const Rect outer = bounds();
  // RTL mirrors the horizontal main axis: the start padding moves to the right edge, and children are
  // laid from the right after the normal placement pass (see the mirror pass at the end).
  const bool mirror = direction() == Direction::Rtl && mAxis == Axis::Row;
  const Insets padding = mirror
    ? Insets{ mPadding.right, mPadding.top, mPadding.left, mPadding.bottom }
    : mPadding;
  const Rect inner = Rect::MakeLTRB(outer.getLeft() + padding.left, outer.getTop() + padding.top,
    outer.getRight() - padding.right, outer.getBottom() - padding.bottom);
  const float innerMain = std::max(0.0f, mAxis == Axis::Row ? inner.getWidth() : inner.getHeight());
  const float innerCross = std::max(0.0f, mAxis == Axis::Row ? inner.getHeight() : inner.getWidth());
  const float innerCrossStart = mAxis == Axis::Row ? inner.getTop() : inner.getLeft();
  const float innerMainStart = mAxis == Axis::Row ? inner.getLeft() : inner.getTop();

  std::vector<FlexItem> items;
  std::vector<std::shared_ptr<Layer>> absolute;
  for (const std::shared_ptr<Layer>& child: children()) {
    if (!child->visible() || child->floating()) {
      continue;
    }
    const LayoutParams& params = child->layoutParams();
    Size childAvailable = childMeasureSpace(mAxis, innerMain, innerCross, params);
    // Unbounded measurement: 0 tells widgets such as text "no width constraint, report your
    // natural size"; margins still count against the other axis.
    if (mAxis == Axis::Row) {
      if (params.unboundedMain) childAvailable.width = 0.0f;
      if (params.unboundedCross) childAvailable.height = 0.0f;
    } else {
      if (params.unboundedMain) childAvailable.height = 0.0f;
      if (params.unboundedCross) childAvailable.width = 0.0f;
    }
    Size childSize = child->measuredSize();
    if (childSize.width <= 0.0f && childSize.height <= 0.0f) {
      childSize = child->measureCached(childAvailable);
    }
    if (params.position == Position::Absolute) {
      absolute.push_back(child);
      continue;
    }
    float childMain = 0.0f;
    float childCross = 0.0f;
    resolveItemSize(params, mAxis, innerMain, innerCross, childSize, &childMain, &childCross,
      intrinsicIfNeeded(*child, params));
    FlexItem item;
    item.layer = child;
    item.margin = params.margin;
    item.main = childMain;
    item.cross = childCross;
    item.grow = std::max(0.0f, params.grow);
    item.shrink = params.unboundedMain ? 0.0f : std::max(0.0f, params.shrink);
    item.minMain = mAxis == Axis::Row ? params.minWidth : params.minHeight;
    item.minCross = mAxis == Axis::Row ? params.minHeight : params.minWidth;
    item.maxMain = mAxis == Axis::Row ? params.maxWidth : params.maxHeight;
    item.maxCross = mAxis == Axis::Row ? params.maxHeight : params.maxWidth;
    item.alignSelf = params.alignSelf;
    items.push_back(std::move(item));
  }

  // Split into lines (one line when wrapping is off). A line's cross extent is its tallest item.
  std::vector<std::vector<size_t>> lines;
  std::vector<float> lineCrossSizes;
  if (mWrap) {
    std::vector<size_t> line;
    float lineMain = 0.0f;
    float lineCross = 0.0f;
    for (size_t index = 0; index < items.size(); ++index) {
      const FlexItem& item = items[index];
      const float marginMain = mAxis == Axis::Row ? item.margin.horizontal() : item.margin.vertical();
      const float marginCross = mAxis == Axis::Row ? item.margin.vertical() : item.margin.horizontal();
      const float addition = item.main + marginMain + (line.empty() ? 0.0f : mGap);
      if (!line.empty() && lineMain + addition > innerMain) {
        lines.push_back(std::move(line));
        lineCrossSizes.push_back(lineCross);
        line.clear();
        lineMain = 0.0f;
        lineCross = 0.0f;
      }
      lineMain += item.main + marginMain + (line.empty() ? 0.0f : mGap);
      lineCross = std::max(lineCross, item.cross + marginCross);
      line.push_back(index);
    }
    if (!line.empty()) {
      lines.push_back(std::move(line));
      lineCrossSizes.push_back(lineCross);
    }
  } else {
    std::vector<size_t> all;
    all.reserve(items.size());
    for (size_t index = 0; index < items.size(); ++index) all.push_back(index);
    lines.push_back(std::move(all));
    lineCrossSizes.push_back(innerCross);
  }

  float lineStart = innerCrossStart;
  for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    std::vector<size_t>& line = lines[lineIndex];
    if (line.empty()) continue;
    const float lineCross = mWrap ? lineCrossSizes[lineIndex] : innerCross;
    // Baseline alignment needs the tallest baseline in the line before placing any item.
    float lineBaseline = 0.0f;
    bool baselineAligned = false;
    for (const size_t index: line) {
      const FlexItem& candidate = items[index];
      if (resolveAlign(candidate.alignSelf, mCrossAlign) != Align::Baseline) continue;
      const float baseline = candidate.layer->baselineOffset();
      if (baseline >= 0.0f) {
        baselineAligned = true;
        lineBaseline = std::max(lineBaseline, baseline);
      }
    }

    // Grow and shrink along the line.
    float used = 0.0f;
    float totalGrow = 0.0f;
    float totalShrink = 0.0f;
    for (const size_t index: line) {
      const FlexItem& item = items[index];
      used += item.main + (mAxis == Axis::Row ? item.margin.horizontal() : item.margin.vertical());
      totalGrow += item.grow;
      totalShrink += item.shrink;
    }
    if (line.size() > 1) used += mGap * static_cast<float>(line.size() - 1);

    if (used < innerMain && totalGrow > 0.0f) {
      const float extra = innerMain - used;
      for (const size_t index: line) {
        FlexItem& item = items[index];
        item.main += extra * (item.grow / totalGrow);
      }
    } else if (used > innerMain && totalShrink > 0.0f) {
      const float overflow = used - innerMain;
      float shrinkWeight = 0.0f;
      for (const size_t index: line) {
        const FlexItem& item = items[index];
        shrinkWeight += item.shrink * item.main;
      }
      if (shrinkWeight > 0.0f) {
        for (const size_t index: line) {
          FlexItem& item = items[index];
          const float reduction = overflow * (item.shrink * item.main / shrinkWeight);
          item.main = std::max(0.0f, item.main - reduction);
        }
      }
    }
    // A min/max clamp refuses space, it does not make it disappear: whatever an item cannot absorb has
    // to go to the items that still can, otherwise the line keeps a gap (or an overflow) the layout
    // could have removed. Four passes are enough for any realistic nesting of min/max.
    const auto clampLine = [&line, &items]() {
      for (const size_t index: line) {
        FlexItem& item = items[index];
        item.main = clamp(item.main, item.minMain, item.maxMain);
        item.cross = clamp(item.cross, item.minCross, item.maxCross);
      }
    };
    const auto lineUsed = [this, &line, &items]() {
      float total = 0.0f;
      for (const size_t index: line) {
        const FlexItem& item = items[index];
        total += item.main + (mAxis == Axis::Row ? item.margin.horizontal() : item.margin.vertical());
      }
      if (line.size() > 1) total += mGap * static_cast<float>(line.size() - 1);
      return total;
    };
    clampLine();
    for (int pass = 0; pass < 4; ++pass) {
      const float delta = innerMain - lineUsed();
      if (std::abs(delta) <= 0.05f) break;
      if (delta > 0.0f) {
        float weight = 0.0f;
        for (const size_t index: line) {
          const FlexItem& item = items[index];
          if (item.grow <= 0.0f) continue;
          if (item.maxMain > 0.0f && item.main >= item.maxMain - 0.05f) continue;
          weight += item.grow;
        }
        if (weight <= 0.0f) break;
        for (const size_t index: line) {
          FlexItem& item = items[index];
          if (item.grow <= 0.0f) continue;
          if (item.maxMain > 0.0f && item.main >= item.maxMain - 0.05f) continue;
          item.main += delta * (item.grow / weight);
        }
      } else {
        const float overflow = -delta;
        float weight = 0.0f;
        for (const size_t index: line) {
          const FlexItem& item = items[index];
          if (item.shrink <= 0.0f) continue;
          if (item.minMain > 0.0f && item.main <= item.minMain + 0.05f) continue;
          weight += item.shrink * item.main;
        }
        if (weight <= 0.0f) break;
        for (const size_t index: line) {
          FlexItem& item = items[index];
          if (item.shrink <= 0.0f) continue;
          if (item.minMain > 0.0f && item.main <= item.minMain + 0.05f) continue;
          const float reduction = overflow * (item.shrink * item.main / weight);
          item.main = std::max(0.0f, item.main - reduction);
        }
      }
      clampLine();
    }

    // The free space justify distributes is what grow/shrink left over.
    float usedAfter = 0.0f;
    for (const size_t index: line) {
      const FlexItem& item = items[index];
      usedAfter += item.main + (mAxis == Axis::Row ? item.margin.horizontal() : item.margin.vertical());
    }
    if (line.size() > 1) usedAfter += mGap * static_cast<float>(line.size() - 1);
    const float free = std::max(0.0f, innerMain - usedAfter);
    float leading = 0.0f;
    float extraGap = 0.0f;
    switch (mJustify) {
      case Justify::Start: break;
      case Justify::Center: leading = free * 0.5f; break;
      case Justify::End: leading = free; break;
      case Justify::SpaceBetween:
        extraGap = line.size() > 1 ? free / static_cast<float>(line.size() - 1) : 0.0f;
        break;
      case Justify::SpaceAround:
        extraGap = free / static_cast<float>(line.size());
        leading = extraGap * 0.5f;
        break;
      case Justify::SpaceEvenly:
        extraGap = free / static_cast<float>(line.size() + 1);
        leading = extraGap;
        break;
    }

    float cursor = innerMainStart + leading;
    for (const size_t index: line) {
      FlexItem& item = items[index];
      const float marginMain =
        mAxis == Axis::Row ? item.margin.horizontal() : item.margin.vertical();
      const float marginCross =
        mAxis == Axis::Row ? item.margin.vertical() : item.margin.horizontal();
      const float crossAvailable = std::max(0.0f, lineCross - marginCross);
      const Align align = resolveAlign(item.alignSelf, mCrossAlign);
      float crossSize = align == Align::Stretch ? crossAvailable : item.cross;
      float crossPosition = lineStart;
      if (mAxis == Axis::Row) {
        crossPosition += item.margin.top;
      } else {
        crossPosition += item.margin.left;
      }
      if (align == Align::Center) {
        crossPosition += (crossAvailable - crossSize) * 0.5f;
      } else if (align == Align::End) {
        crossPosition += crossAvailable - crossSize;
      } else if (align == Align::Baseline && baselineAligned) {
        const float baseline = item.layer->baselineOffset();
        if (baseline >= 0.0f) crossPosition += lineBaseline - baseline;
      }

      const float mainStart = cursor +
        (mAxis == Axis::Row ? item.margin.left : item.margin.top);
      const Rect childBounds = mAxis == Axis::Row
        ? Rect::MakeXYWH(mainStart, crossPosition, item.main, crossSize)
        : Rect::MakeXYWH(crossPosition, mainStart, crossSize, item.main);
      // Tell the child whether this size came from the parent's distribution or from its own
      // measurement. A grow item, a basis, a percentage and a stretched cross axis are assigned; a
      // fit-content item is not, because the parent used what the child measured to place it. The
      // distinction is what lets `hasFixedParentSize()` treat a flex-assigned viewport as a layout
      // boundary without lying about content-sized children.
      const LayoutParams& params = item.layer->layoutParams();
      const bool mainAssigned = !params.fitContentMain && !params.unboundedMain &&
        (item.grow > 0.0f || params.basis >= 0.0f ||
          (mAxis == Axis::Row ? params.widthPercent : params.heightPercent) >= 0.0f);
      const bool crossAssigned = !params.fitContentCross && !params.unboundedCross &&
        ((!mWrap && align == Align::Stretch) || params.aspectRatio > 0.0f ||
          (mAxis == Axis::Row ? params.heightPercent : params.widthPercent) >= 0.0f);
      item.layer->noteParentAssignedSize(mainAssigned && crossAssigned);
      item.layer->setBounds(childBounds);
      item.layer->layout();
      cursor += marginMain + item.main + mGap + extraGap;
    }

    lineStart += lineCross + (mWrap ? (mLineGap > 0.0f ? mLineGap : mGap) : 0.0f);
  }

  for (const std::shared_ptr<Layer>& child: absolute) {
    layoutAbsolute(*child, inner);
  }

  // RTL mirror pass: the LTR placement above is direction-agnostic, so mirroring every non-absolute
  // child across the content box is enough to obtain right-to-left order without duplicating the
  // grow/shrink/justify math. Absolute children keep their explicit insets.
  if (mirror) {
    for (const std::shared_ptr<Layer>& child: children()) {
      if (!child->visible() || child->layoutParams().position == Position::Absolute) continue;
      const Rect placed = child->bounds();
      const float mirroredX = inner.getLeft() + inner.getRight() - placed.getRight();
      if (mirroredX != placed.getLeft()) {
        child->setBounds(Rect::MakeXYWH(mirroredX, placed.getTop(), placed.getWidth(),
          placed.getHeight()));
      }
    }
  }
  clearNeedsLayout();
}


Size Flex::intrinsicSize() const {
  float main = 0.0f;
  float cross = 0.0f;
  size_t count = 0;
  for (const std::shared_ptr<Layer>& child: children()) {
    if (!child->visible() || child->layoutParams().position == Position::Absolute) continue;
    Size childSize = child->intrinsicSize();
    if (childSize.width < 0.0f || childSize.height < 0.0f) {
      childSize = child->measuredSize();
      if (childSize.width <= 0.0f && childSize.height <= 0.0f) {
        childSize = child->measure({});
      }
    }
    const LayoutParams& params = child->layoutParams();
    const float childMain = mAxis == Axis::Row ? childSize.width : childSize.height;
    const float childCross = mAxis == Axis::Row ? childSize.height : childSize.width;
    main += childMain + (mAxis == Axis::Row ? params.margin.horizontal() : params.margin.vertical());
    cross = std::max(cross,
      childCross + (mAxis == Axis::Row ? params.margin.vertical() : params.margin.horizontal()));
    ++count;
  }
  if (count == 0) return Size{ mPadding.horizontal(), mPadding.vertical() };
  main += mGap * static_cast<float>(count - 1);
  return mAxis == Axis::Row
    ? Size{ main + mPadding.horizontal(), cross + mPadding.vertical() }
    : Size{ cross + mPadding.horizontal(), main + mPadding.vertical() };
}
} // namespace coral::ui
