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

#include <ui/Table.h>
#include <ui/Theme.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui {

namespace {

void replaceAll(std::string& text, const std::string& needle, const std::string& value) {
  if (needle.empty()) return;
  for (std::size_t at = text.find(needle); at != std::string::npos;
       at = text.find(needle, at + value.size())) {
    text.replace(at, needle.size(), value);
  }
}

} // namespace

Table::Table(std::vector<TableColumn> columns) : mColumns(std::move(columns)) {
  setClip(true);
  // A table is a semantic container, not just decorative cells, so AX bridges can announce it.
  setAccessibleRole(AccessibilityRole::Table);
}

void Table::setColumns(std::vector<TableColumn> columns) {
  mColumns = std::move(columns);
  setNeedsLayout();
}

void Table::setGap(const float gap) {
  if (mGap == gap) return;
  mGap = std::max(0.0f, gap);
  setNeedsLayout();
  setNeedsPaint();
}

void Table::setPadding(const Insets& padding) {
  mPadding = padding;
  setNeedsLayout();
}

void Table::setRows(std::vector<std::vector<std::shared_ptr<Layer>>> rows) {
  for (const auto& row: mRows) {
    for (const auto& cell: row) {
      if (cell != nullptr) removeChild(cell);
    }
  }
  mRows = std::move(rows);
  for (const auto& row: mRows) {
    for (const auto& cell: row) {
      if (cell != nullptr) addChild(cell);
    }
  }
  setNeedsLayout();
  setNeedsPaint();
}

std::vector<float> Table::resolveColumnWidths(const float availableWidth) const {
  std::vector<float> widths;
  widths.reserve(mColumns.size());
  if (mColumns.empty()) return widths;
  const float gaps = mGap * static_cast<float>(mColumns.size() - 1);
  const float content = std::max(0.0f, availableWidth - mPadding.horizontal() - gaps);
  float fixed = 0.0f;
  float totalWeight = 0.0f;
  float percentSum = 0.0f;
  for (const TableColumn& column: mColumns) {
    if (column.width > 0.0f) {
      fixed += column.width;
    } else if (column.percent > 0.0f) {
      percentSum += column.percent;
    } else {
      totalWeight += std::max(0.0f, column.weight);
    }
  }
  // Percentage columns claim their share of the content width first (clamped to 100%); the rest is
  // distributed by weight, which keeps the existing fixed/weight behaviour intact.
  const float percentTotal = std::min(1.0f, percentSum);
  const float percentWidth = content * percentTotal;
  const float flexible = std::max(0.0f, content - fixed - percentWidth);
  for (const TableColumn& column: mColumns) {
    if (column.width > 0.0f) {
      widths.push_back(column.width);
    } else if (column.percent > 0.0f) {
      widths.push_back(percentTotal > 0.0f ? percentWidth * (column.percent / percentSum) : 0.0f);
    } else if (totalWeight > 0.0f) {
      widths.push_back(flexible * std::max(0.0f, column.weight) / totalWeight);
    } else {
      widths.push_back(0.0f);
    }
  }
  return widths;
}

std::optional<AccessibleGrid> Table::accessibleGrid() const {
  if (mRows.empty()) return std::nullopt;
  int columns = 0;
  for (const auto& row: mRows) columns = std::max(columns, static_cast<int>(row.size()));
  if (columns == 0) return std::nullopt;
  return AccessibleGrid{ static_cast<int>(mRows.size()), columns };
}

std::vector<float> Table::resolveRowHeights() const {
  return resolveRowHeights(resolveColumnWidths(bounds().getWidth()));
}

std::vector<float> Table::resolveRowHeights(const std::vector<float>& widths) const {
  std::vector<float> heights;
  heights.reserve(mRows.size());
  for (const auto& row: mRows) {
    float height = 0.0f;
    for (size_t column = 0; column < row.size(); ++column) {
      const std::shared_ptr<Layer>& cell = row[column];
      if (cell == nullptr || !cell->visible()) continue;
      // Measure at the column width, not unbounded: a wrapping cell reports one line when asked with
      // no width and then overflows a narrow column, because layout only assigns bounds and never
      // re-measures it.
      const float width = column < widths.size() ? widths[column] : 0.0f;
      height = std::max(height, cell->measureCached(Size{ width, 0.0f }).height);
    }
    heights.push_back(height);
  }
  return heights;
}

Size Table::measure(const Size& available) {
  const std::vector<float> widths = resolveColumnWidths(available.width);
  const std::vector<float> heights = resolveRowHeights(widths);
  float width = mPadding.horizontal();
  for (const float column: widths) width += column;
  width += mGap * static_cast<float>(std::max<size_t>(0, widths.size() - 1));
  float height = mPadding.vertical();
  for (const float row: heights) height += row;
  height += mGap * static_cast<float>(std::max<size_t>(0, heights.size() - 1));
  const Size measured{ width, height };
  setMeasuredSize(measured);
  return measured;
}

void Table::layout() {
  const std::vector<float> widths = resolveColumnWidths(bounds().getWidth());
  const std::vector<float> heights = resolveRowHeights(widths);
  float y = bounds().getTop() + mPadding.top;
  for (size_t rowIndex = 0; rowIndex < mRows.size(); ++rowIndex) {
    const float rowHeight = rowIndex < heights.size() ? heights[rowIndex] : 0.0f;
    float x = bounds().getLeft() + mPadding.left;
    for (size_t column = 0; column < mRows[rowIndex].size() && column < widths.size(); ++column) {
      const std::shared_ptr<Layer>& cell = mRows[rowIndex][column];
      if (cell != nullptr) {
        cell->setBounds(Rect::MakeXYWH(x, y, widths[column], rowHeight));
        // A cell that is itself a container has to arrange its children at the assigned rectangle;
        // `setBounds` only moves the box, and nothing else calls the child's layout pass.
        if (cell->needsLayout() || cell->subtreeNeedsLayout()) cell->layout();
        // Give an unnamed cell a positional accessible name so a bridge can announce "row 2,
        // column 1" instead of an empty node; an explicit name from the caller always wins. The
        // pattern comes from the theme's translator, so a localized screen reader hears it in its own
        // language.
        if (cell->accessibleName().empty()) {
          std::string name = tr("ui.table.cell", "Row {row}, column {column}");
          replaceAll(name, "{row}", std::to_string(rowIndex + 1));
          replaceAll(name, "{column}", std::to_string(column + 1));
          cell->setAccessibleName(std::move(name));
        }
      }
      x += widths[column] + mGap;
    }
    y += rowHeight + mGap;
  }
  clearNeedsLayout();
}


Rect Table::rowBounds(const int row) const {
  const std::vector<float> heights = resolveRowHeights();
  if (row < 0 || row >= static_cast<int>(heights.size())) return {};
  float top = bounds().getTop() + mPadding.top;
  for (int index = 0; index < row; ++index) {
    top += heights[static_cast<size_t>(index)] + mGap;
  }
  return Rect::MakeXYWH(bounds().getLeft() + mPadding.left, top,
    std::max(0.0f, bounds().getWidth() - mPadding.horizontal()),
    heights[static_cast<size_t>(row)]);
}

void Table::setSelectionColor(const Color color) {
  mSelectionColor = color;
  mSelectionColorExplicit = true;
  setNeedsPaint();
}

void Table::applyTheme(const Theme& theme) {
  if (!mSelectionColorExplicit) {
    mSelectionColor = theme.accent;
    mSelectionColor.alpha = 48.0f / 255.0f;
  }
  if (!mHoverColorExplicit) {
    mHoverColor = theme.controls.backgroundHover;
    mHoverColor.alpha = 40.0f / 255.0f;
  }
  mFocusColor = theme.accent;
  mFocusColor.alpha = 200.0f / 255.0f;
  setNeedsPaint();
}

void Table::setSelectedRow(const int row) {
  const int rows = static_cast<int>(resolveRowHeights().size());
  const int next = row >= 0 && row < rows ? row : -1;
  if (next == mSelectedRow) return;
  mSelectedRow = next;
  setNeedsPaint();
}

void Table::drawSelf(RenderContext& context) {
  if (mHoveredRow >= 0 && mHoverColor.alpha > 0.0f) {
    const Rect hovered = rowBounds(mHoveredRow);
    if (!hovered.isEmpty()) {
      graphics::Paint paint;
      paint.color = mHoverColor;
      paint.style = graphics::Paint::Style::kFill;
      context.canvas().drawRect(hovered, paint);
    }
  }
  if (mFocused) {
    graphics::Paint outline;
    outline.color = mFocusColor;
    outline.style = graphics::Paint::Style::kStroke;
    outline.strokeWidth = 1.5f;
    context.canvas().drawRoundRect(
      graphics::RoundRect::MakeRectRadius(bounds(), 3.0f), outline);
  }
  if (mSelectedRow < 0 || mSelectionColor.alpha <= 0.0f) return;
  const Rect selected = rowBounds(mSelectedRow);
  if (selected.isEmpty()) return;
  graphics::Paint highlight;
  highlight.color = mSelectionColor;
  highlight.style = graphics::Paint::Style::kFill;
  context.canvas().drawRect(selected, highlight);
}

int Table::rowAtLocal(const Point& point) const {
  const std::vector<float> heights = resolveRowHeights();
  float top = mPadding.top;
  const float localX = point.x - bounds().getLeft();
  const float localY = point.y - bounds().getTop();
  if (localX < mPadding.left || localX > bounds().getWidth() - mPadding.right) return -1;
  for (size_t index = 0; index < heights.size(); ++index) {
    if (localY >= top && localY < top + heights[index]) return static_cast<int>(index);
    top += heights[index] + mGap;
  }
  return -1;
}

void Table::setHoveredRow(const int row) {
  const int rows = static_cast<int>(resolveRowHeights().size());
  const int next = row >= 0 && row < rows ? row : -1;
  if (next == mHoveredRow) return;
  mHoveredRow = next;
  setNeedsPaint();
}

void Table::setHoverColor(const Color color) {
  mHoverColor = color;
  mHoverColorExplicit = true;
  setNeedsPaint();
}

void Table::setFocused(const bool focused) {
  if (mFocused == focused) return;
  mFocused = focused;
  setNeedsPaint();
}
} // namespace coral::ui
