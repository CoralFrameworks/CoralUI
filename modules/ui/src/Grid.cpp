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

#include <ui/Grid.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace coral::ui {

namespace {

struct CellPlacement {
  std::shared_ptr<Layer> child;
  int row = 0;
  int column = 0;
  int columnSpan = 1;
  int rowSpan = 1;
};

void ensureRows(std::vector<std::vector<uint8_t>>& occupied, const size_t rows,
    const size_t columns) {
  while (occupied.size() < rows) occupied.emplace_back(columns, 0);
}

std::vector<CellPlacement> placeChildren(const Grid& grid,
    const std::vector<std::shared_ptr<Layer>>& visible) {
  const int columns = std::max(1, grid.columns());
  std::vector<std::vector<uint8_t>> occupied;
  std::vector<CellPlacement> placements;
  placements.reserve(visible.size());
  for (const auto& child: visible) {
    const Grid::Span span = grid.span(*child);
    const int columnSpan = std::clamp(span.columns, 1, columns);
    const int rowSpan = std::max(1, span.rows);
    int placedRow = 0;
    int placedColumn = 0;
    bool found = false;
    for (int row = 0; !found; ++row) {
      ensureRows(occupied, static_cast<size_t>(row + rowSpan), static_cast<size_t>(columns));
      for (int column = 0; column + columnSpan <= columns; ++column) {
        bool free = true;
        for (int r = 0; r < rowSpan && free; ++r) {
          for (int c = 0; c < columnSpan; ++c) {
            if (occupied[static_cast<size_t>(row + r)][static_cast<size_t>(column + c)] != 0) {
              free = false;
              break;
            }
          }
        }
        if (!free) continue;
        for (int r = 0; r < rowSpan; ++r) {
          for (int c = 0; c < columnSpan; ++c) {
            occupied[static_cast<size_t>(row + r)][static_cast<size_t>(column + c)] = 1;
          }
        }
        placedRow = row;
        placedColumn = column;
        found = true;
        break;
      }
    }
    placements.push_back(CellPlacement{ child, placedRow, placedColumn, columnSpan, rowSpan });
  }
  return placements;
}

std::vector<std::shared_ptr<Layer>> visibleChildren(const Grid& grid) {
  std::vector<std::shared_ptr<Layer>> visible;
  for (const auto& child: grid.children()) {
    if (child->visible()) visible.push_back(child);
  }
  return visible;
}

} // namespace

Grid::Grid(const int columns) : mColumns(std::max(1, columns)) {}

void Grid::setColumns(const int columns) {
  const int next = std::max(1, columns);
  if (mColumns == next) {
    return;
  }
  mColumns = next;
  setNeedsLayout();
}

void Grid::setGap(const float gap) {
  const float next = std::max(0.0f, gap);
  if (mGap == next) {
    return;
  }
  mGap = next;
  setNeedsLayout();
}

void Grid::setPadding(const Insets& padding) {
  if (mPadding == padding) return;
  mPadding = padding;
  setNeedsLayout();
}

void Grid::setSpan(const std::shared_ptr<Layer>& child, const int columns, const int rows) {
  if (child == nullptr) return;
  Span span;
  span.columns = std::max(1, columns);
  span.rows = std::max(1, rows);
  mSpans[child.get()] = span;
  setNeedsLayout();
}

Grid::Span Grid::span(const Layer& child) const noexcept {
  const auto found = mSpans.find(&child);
  return found == mSpans.end() ? Span{} : found->second;
}

Size Grid::measure(const Size& available) {
  const float innerWidth = std::max(0.0f, available.width - mPadding.horizontal());
  const float innerHeight = std::max(0.0f, available.height - mPadding.vertical());
  const bool bounded = available.width > 0.0f;
  float cellWidth = bounded
    ? std::max(0.0f, (innerWidth - mGap * static_cast<float>(mColumns - 1)) /
        static_cast<float>(mColumns))
    : 0.0f;

  const std::vector<std::shared_ptr<Layer>> visible = visibleChildren(*this);
  const std::vector<CellPlacement> placements = placeChildren(*this, visible);
  std::vector<float> rowHeights;

  const auto measureChildren = [&](const float width) {
    for (const CellPlacement& placement: placements) {
      const float spanWidth = width * static_cast<float>(placement.columnSpan) +
        mGap * static_cast<float>(placement.columnSpan - 1);
      const Size childSize = placement.child->measureCached({ spanWidth, std::max(0.0f, innerHeight) });
      const size_t lastRow = static_cast<size_t>(placement.row + placement.rowSpan);
      while (rowHeights.size() < lastRow) rowHeights.push_back(0.0f);
      if (placement.rowSpan == 1) {
        rowHeights[static_cast<size_t>(placement.row)] = std::max(
          rowHeights[static_cast<size_t>(placement.row)], childSize.height);
      } else {
        const float needed = std::max(0.0f, childSize.height -
          mGap * static_cast<float>(placement.rowSpan - 1)) / static_cast<float>(placement.rowSpan);
        for (int row = 0; row < placement.rowSpan; ++row) {
          rowHeights[static_cast<size_t>(placement.row + row)] = std::max(
            rowHeights[static_cast<size_t>(placement.row + row)], needed);
        }
      }
    }
  };

  measureChildren(cellWidth);
  if (!bounded) {
    float naturalCell = 0.0f;
    for (const CellPlacement& placement: placements) {
      const float childWidth = placement.child->measuredSize().width;
      const float needed = std::max(0.0f, childWidth -
        mGap * static_cast<float>(placement.columnSpan - 1)) /
        static_cast<float>(placement.columnSpan);
      naturalCell = std::max(naturalCell, needed);
    }
    cellWidth = naturalCell;
    rowHeights.clear();
    measureChildren(cellWidth);
  }

  float height = 0.0f;
  for (const float rowHeight: rowHeights) height += rowHeight;
  if (rowHeights.size() > 1) height += mGap * static_cast<float>(rowHeights.size() - 1);
  const float width = bounded
    ? available.width
    : cellWidth * static_cast<float>(mColumns) + mGap * static_cast<float>(mColumns - 1) +
      mPadding.horizontal();
  setMeasuredSize({ width, height + mPadding.vertical() });
  return measuredSize();
}

void Grid::layout() {
  // RTL mirrors the column order and swaps the horizontal padding, matching a Row Flex.
  const bool rtl = direction() == Direction::Rtl;
  const Insets padding = rtl
    ? Insets{ mPadding.right, mPadding.top, mPadding.left, mPadding.bottom }
    : mPadding;
  const Rect inner = Rect::MakeLTRB(bounds().getLeft() + padding.left,
    bounds().getTop() + padding.top, bounds().getRight() - padding.right,
    bounds().getBottom() - padding.bottom);
  const float cellWidth = std::max(0.0f,
    (inner.getWidth() - mGap * static_cast<float>(mColumns - 1)) /
      static_cast<float>(mColumns));

  const std::vector<std::shared_ptr<Layer>> visible = visibleChildren(*this);
  const std::vector<CellPlacement> placements = placeChildren(*this, visible);
  std::vector<float> rowHeights;
  for (const CellPlacement& placement: placements) {
    const size_t lastRow = static_cast<size_t>(placement.row + placement.rowSpan);
    while (rowHeights.size() < lastRow) rowHeights.push_back(0.0f);
    const float childHeight = placement.child->measuredSize().height > 0.0f
      ? placement.child->measuredSize().height
      : placement.child->bounds().getHeight();
    if (placement.rowSpan == 1) {
      rowHeights[static_cast<size_t>(placement.row)] = std::max(
        rowHeights[static_cast<size_t>(placement.row)], childHeight);
    } else {
      const float needed = std::max(0.0f, childHeight -
        mGap * static_cast<float>(placement.rowSpan - 1)) / static_cast<float>(placement.rowSpan);
      for (int row = 0; row < placement.rowSpan; ++row) {
        rowHeights[static_cast<size_t>(placement.row + row)] = std::max(
          rowHeights[static_cast<size_t>(placement.row + row)], needed);
      }
    }
  }

  std::vector<float> rowTops(rowHeights.size());
  float y = inner.getTop();
  for (size_t row = 0; row < rowHeights.size(); ++row) {
    rowTops[row] = y;
    y += rowHeights[row] + mGap;
  }
  for (const CellPlacement& placement: placements) {
    const float x = inner.getLeft() + static_cast<float>(placement.column) * (cellWidth + mGap);
    const float width = cellWidth * static_cast<float>(placement.columnSpan) +
      mGap * static_cast<float>(placement.columnSpan - 1);
    const float top = rowTops[static_cast<size_t>(placement.row)];
    float height = 0.0f;
    for (int row = 0; row < placement.rowSpan; ++row) {
      height += rowHeights[static_cast<size_t>(placement.row + row)];
    }
    if (placement.rowSpan > 1) height += mGap * static_cast<float>(placement.rowSpan - 1);
    // Percentage sizing is relative to the cell rectangle, which is what a span/cell layout means by
    // "50%". Unset percentages keep the cell-filling behaviour.
    const LayoutParams& params = placement.child->layoutParams();
    float childWidth = width;
    float childHeight = height;
    if (params.widthPercent > 0.0f) childWidth = std::max(0.0f, width * params.widthPercent);
    if (params.heightPercent > 0.0f) childHeight = std::max(0.0f, height * params.heightPercent);
    const float placedX = rtl ? inner.getLeft() + inner.getRight() - (x + width) : x;
    const float childX = rtl ? placedX + (width - childWidth) : placedX;
    placement.child->setBounds(Rect::MakeXYWH(childX, top, childWidth, childHeight));
    placement.child->layout();
  }
  clearNeedsLayout();
}

} // namespace coral::ui
