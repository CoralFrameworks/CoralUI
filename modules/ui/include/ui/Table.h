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

#ifndef CORAL_UI_TABLE_H
#define CORAL_UI_TABLE_H

#include <ui/Geometry.h>
#include <ui/Layer.h>

#include <memory>
#include <vector>

namespace coral::ui {

/** One table column: a fixed width when set, otherwise a share of the remaining width. */
struct TableColumn {
  /** Fixed width in logical units; 0 means "use `weight`". */
  float width = 0.0f;
  /** Share of the leftover width (only used when `width` is 0). */
  float weight = 1.0f;
  /** Fraction of the content width (0..1); takes precedence over `weight` when set. */
  float percent = 0.0f;
};

/**
 * A column-aligned table.
 *
 * Cells are explicit layers, one vector per row; a null cell leaves a hole. Column widths are the
 * fixed widths first, then the leftover width is split by weight, which is what an Inspector or a
 * media list needs without a full grid solver. Rows size themselves to their tallest cell and the
 * table reports the resulting content height during measure.
 *
 * The table owns no cells: `setRows` inserts the given layers as children and drops the previous
 * ones. Reuse cell layers across updates to keep focus and animations alive. Cells keep their own
 * colours: a `TextLayer` created without an explicit colour defaults to white, so build cells with
 * `theme.controls.foreground` (and the matching muted colour) when the table must follow a skin.
 */
class Table final : public Layer {
 public:
  explicit Table(std::vector<TableColumn> columns = {});

  void setColumns(std::vector<TableColumn> columns);
  [[nodiscard]] const std::vector<TableColumn>& columns() const noexcept {
    return mColumns;
  }

  void setGap(float gap);
  [[nodiscard]] float gap() const noexcept {
    return mGap;
  }
  void setPadding(const Insets& padding);
  [[nodiscard]] const Insets& padding() const noexcept {
    return mPadding;
  }

  void setRows(std::vector<std::vector<std::shared_ptr<Layer>>> rows);
  [[nodiscard]] const std::vector<std::vector<std::shared_ptr<Layer>>>& rows() const noexcept {
    return mRows;
  }

  Size measure(const Size& available) override;
  void layout() override;
  void drawSelf(RenderContext& context) override;

  /** Column widths for @p availableWidth, exposed for tests and diagnostics. */
  [[nodiscard]] std::vector<float> resolveColumnWidths(float availableWidth) const;
  /** Row heights from the cells' measured sizes. */
  /** A table is a boundary only when its parent pinned its size. */
  [[nodiscard]] bool isLayoutBoundary() const override {
    return hasFixedParentSize();
  }
  [[nodiscard]] std::optional<AccessibleGrid> accessibleGrid() const override;
  [[nodiscard]] std::vector<float> resolveRowHeights() const;
  /** Row heights for an explicit column layout: cells are measured at their column width. */
  [[nodiscard]] std::vector<float> resolveRowHeights(const std::vector<float>& widths) const;
  /** The rectangle of @p row in table coordinates; empty when the row does not exist. */
  [[nodiscard]] Rect rowBounds(int row) const;
  /** Highlight colour for the selected row; a transparent colour disables the highlight. */
  void setSelectionColor(Color color);
  /** Marks @p row as selected; out-of-range values clear the selection. */
  void setSelectedRow(int row);
  [[nodiscard]] int selectedRow() const noexcept {
    return mSelectedRow;
  }
  /** Row under a table-local point (rowBounds coordinates), or -1. */
  [[nodiscard]] int rowAtLocal(const Point& point) const;
  void setHoveredRow(int row);
  [[nodiscard]] int hoveredRow() const noexcept {
    return mHoveredRow;
  }
  void setHoverColor(Color color);
  /** Draws a focus outline; a focused table is the keyboard target of its view. */
  void setFocused(bool focused);
  [[nodiscard]] bool focused() const noexcept {
    return mFocused;
  }

  [[nodiscard]] const char* defaultStyleName() const override {
    return "table";
  }

 private:
  void applyTheme(const Theme& theme) override;

  std::vector<TableColumn> mColumns;
  std::vector<std::vector<std::shared_ptr<Layer>>> mRows;
  float mGap = 8.0f;
  int mSelectedRow = -1;
  int mHoveredRow = -1;
  Color mSelectionColor = Color::MakeRGBA8(86, 156, 255, 48);
  Color mHoverColor = Color::MakeRGBA8(255, 255, 255, 20);
  Color mFocusColor = Color::MakeRGBA8(86, 156, 255, 200);
  bool mSelectionColorExplicit = false;
  bool mHoverColorExplicit = false;
  bool mFocused = false;
  Insets mPadding{};
};

} // namespace coral::ui

#endif // CORAL_UI_TABLE_H
