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

#ifndef CORAL_UI_TABLEVIEW_H
#define CORAL_UI_TABLEVIEW_H

#include <ui/ScrollView.h>
#include <ui/Table.h>

#include <functional>
#include <memory>
#include <vector>

namespace coral::ui {

/**
 * A scrollable, selectable table.
 *
 * This is the composition the applications kept repeating: a `Table` inside a `ScrollView` plus row
 * selection. The view owns the table as its content, forwards the column/row model, and translates a
 * press into a row index. Highlights are left to the cell layers or the owner's row state, because a
 * table cell can be any control and forcing a background colour would fight its own style.
 */
class TableView final : public ScrollView {
 public:
  TableView();

  void setColumns(std::vector<TableColumn> columns);
  [[nodiscard]] const std::vector<TableColumn>& columns() const noexcept;
  void setRows(std::vector<std::vector<std::shared_ptr<Layer>>> rows);
  [[nodiscard]] const std::vector<std::vector<std::shared_ptr<Layer>>>& rows() const noexcept;

  void setGap(float gap);
  void setPadding(const Insets& padding);

  void setSelectedRow(int row);
  [[nodiscard]] int selectedRow() const noexcept {
    return mSelectedRow;
  }
  void setOnRowActivated(std::function<void(int)> callback);

  [[nodiscard]] Table* table() noexcept {
    return mTable.get();
  }

  bool onPointerDown(const PointerEvent& event) override;
  /** Tracks the row under the pointer so the table can paint a hover highlight. */
  bool onPointerMove(const PointerEvent& event) override;
  /** Mirrors focus onto the table so it shows the keyboard-focus outline. */
  void onFocusChanged(bool focused) override;

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  /** Arrow/Home/End/Page keys move the selection; Enter activates the selected row. */
  bool onKeyDown(const KeyEvent& event) override;
  /** Scrolls the view so @p row is fully visible. */
  void ensureRowVisible(int row);

 private:
  std::shared_ptr<Table> mTable;
  int mSelectedRow = -1;
  std::function<void(int)> mOnRowActivated;
};

} // namespace coral::ui

#endif // CORAL_UI_TABLEVIEW_H
