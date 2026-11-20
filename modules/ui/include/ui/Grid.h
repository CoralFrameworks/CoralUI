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

#ifndef CORAL_UI_GRID_H
#define CORAL_UI_GRID_H

#include <ui/Layer.h>

#include <memory>
#include <unordered_map>

namespace coral::ui {

/**
 * A simple equal-column grid.
 *
 * Children are placed row-major into `columns` cells; each row is as tall as its tallest measured
 * child. A child can occupy multiple columns/rows through `setSpan`. It is intentionally the layout
 * primitive an Inspector/settings form needs, not a CSS grid.
 */
class Grid final : public Layer {
 public:
  /** How many grid cells a child occupies. */
  struct Span {
    int columns = 1;
    int rows = 1;
  };

  explicit Grid(int columns = 2);

  void setColumns(int columns);
  [[nodiscard]] int columns() const noexcept {
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

  /** Sets the column/row span for @p child; spans are clamped to the grid width. */
  void setSpan(const std::shared_ptr<Layer>& child, int columns, int rows = 1);
  [[nodiscard]] Span span(const Layer& child) const noexcept;

  Size measure(const Size& available) override;
  void layout() override;
  /** A grid is a boundary only when its parent pinned its size. */
  [[nodiscard]] bool isLayoutBoundary() const override {
    return hasFixedParentSize();
  }

 private:
  int mColumns = 2;
  float mGap = 8.0f;
  Insets mPadding{};
  std::unordered_map<const Layer*, Span> mSpans;
};

} // namespace coral::ui

#endif // CORAL_UI_GRID_H
