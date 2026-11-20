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

#include <ui/TableView.h>

#include <algorithm>

namespace coral::ui {

TableView::TableView() {
  mTable = std::make_shared<Table>();
  setContent(mTable);
}

void TableView::setColumns(std::vector<TableColumn> columns) {
  mTable->setColumns(std::move(columns));
  setNeedsLayout();
}

const std::vector<TableColumn>& TableView::columns() const noexcept {
  static const std::vector<TableColumn> empty;
  return mTable != nullptr ? mTable->columns() : empty;
}

void TableView::setRows(std::vector<std::vector<std::shared_ptr<Layer>>> rows) {
  mTable->setRows(std::move(rows));
  if (mSelectedRow >= static_cast<int>(mTable->rows().size())) mSelectedRow = -1;
  setNeedsLayout();
}

const std::vector<std::vector<std::shared_ptr<Layer>>>& TableView::rows() const noexcept {
  static const std::vector<std::vector<std::shared_ptr<Layer>>> empty;
  return mTable != nullptr ? mTable->rows() : empty;
}

void TableView::setGap(const float gap) {
  mTable->setGap(gap);
  setNeedsLayout();
}

void TableView::setPadding(const Insets& padding) {
  mTable->setPadding(padding);
  setNeedsLayout();
}

void TableView::setSelectedRow(const int row) {
  const int next = row >= 0 && row < static_cast<int>(mTable->rows().size()) ? row : -1;
  if (next == mSelectedRow) return;
  mSelectedRow = next;
  // Keep the table's highlight in sync with the view's selection model.
  mTable->setSelectedRow(next);
  setNeedsPaint();
}

void TableView::setOnRowActivated(std::function<void(int)> callback) {
  mOnRowActivated = std::move(callback);
}

bool TableView::onPointerDown(const PointerEvent& event) {
  if (!bounds().contains(event.position)) return false;
  const Rect tableBounds = mTable->bounds();
  const float localY = event.position.y - tableBounds.getTop();
  float top = mTable->padding().top;
  const int count = static_cast<int>(mTable->rows().size());
  for (int row = 0; row < count; ++row) {
    float height = 0.0f;
    for (const auto& cell: mTable->rows()[static_cast<size_t>(row)]) {
      if (cell != nullptr && cell->visible()) height = std::max(height, cell->bounds().getHeight());
    }
    if (localY >= top && localY < top + height) {
      setSelectedRow(row);
      if (mOnRowActivated) mOnRowActivated(row);
      return true;
    }
    top += height + mTable->gap();
  }
  return false;
}


void TableView::ensureRowVisible(const int row) {
  const int count = static_cast<int>(mTable->rows().size());
  if (row < 0 || row >= count) return;
  const int selected = std::clamp(row, 0, count - 1);
  float top = mTable->padding().top;
  for (int index = 0; index < selected; ++index) {
    float height = 0.0f;
    for (const auto& cell: mTable->rows()[static_cast<size_t>(index)]) {
      if (cell != nullptr && cell->visible()) height = std::max(height, cell->bounds().getHeight());
    }
    top += height + mTable->gap();
  }
  float height = 0.0f;
  for (const auto& cell: mTable->rows()[static_cast<size_t>(selected)]) {
    if (cell != nullptr && cell->visible()) height = std::max(height, cell->bounds().getHeight());
  }
  const float viewport = bounds().getHeight();
  if (viewport <= 0.0f) return;
  const float current = scrollY();
  if (top < current) {
    setScrollOffset(scrollX(), top);
  } else if (top + height > current + viewport) {
    setScrollOffset(scrollX(), top + height - viewport);
  }
}

bool TableView::onKeyDown(const KeyEvent& event) {
  const int count = static_cast<int>(mTable->rows().size());
  if (count == 0) return false;
  int row = mSelectedRow;
  switch (event.key) {
    case Key::Down: row = std::min(row + 1, count - 1); break;
    case Key::Up: row = std::max(row - 1, 0); break;
    case Key::Home: row = 0; break;
    case Key::End: row = count - 1; break;
    case Key::PageDown: row = std::min(row + 10, count - 1); break;
    case Key::PageUp: row = std::max(row - 10, 0); break;
    case Key::Enter:
      if (mSelectedRow >= 0 && mOnRowActivated) mOnRowActivated(mSelectedRow);
      return mSelectedRow >= 0;
    default:
      return false;
  }
  setSelectedRow(row);
  ensureRowVisible(mSelectedRow);
  return true;
}

bool TableView::onPointerMove(const PointerEvent& event) {
  const Rect tableBounds = mTable->bounds();
  const Point local{ event.position.x, event.position.y };
  (void)tableBounds;
  mTable->setHoveredRow(mTable->rowAtLocal(local));
  return ScrollView::onPointerMove(event);
}

void TableView::onFocusChanged(const bool focused) {
  ScrollView::onFocusChanged(focused);
  mTable->setFocused(focused);
}
} // namespace coral::ui
