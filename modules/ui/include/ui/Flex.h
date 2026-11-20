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

#ifndef CORAL_UI_FLEX_H
#define CORAL_UI_FLEX_H

#include <ui/Layer.h>

namespace coral::ui {

enum class Axis : uint8_t {
  Row,
  Column,
};

enum class Align : uint8_t {
  Start,
  Center,
  End,
  Stretch,
  /** Align the children's first text baselines instead of their boxes. */
  Baseline,
};

/** How leftover main-axis space is distributed between the items of a line. */
enum class Justify : uint8_t {
  Start,
  Center,
  End,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly,
};

/**
 * A one-dimensional flex container: children in a row or a column, with padding, a gap and one cross
 * axis alignment rule.
 *
 * This is deliberately the subset the editor chrome needs -- toolbar rows, side panels, form stacks --
 * rather than the CSS model. `measure` gives every child the inner available space and sums the main
 * axis; `layout` assigns absolute rectangles, so a nested tree is arranged top-down.
 */
class Flex final : public Layer {
 public:
  explicit Flex(Axis axis = Axis::Row);

  void setAxis(Axis axis);
  [[nodiscard]] Axis axis() const noexcept {
    return mAxis;
  }

  void setPadding(const Insets& padding);
  [[nodiscard]] const Insets& padding() const noexcept {
    return mPadding;
  }

  void setGap(float gap);
  [[nodiscard]] float gap() const noexcept {
    return mGap;
  }

  void setCrossAlign(Align align);
  [[nodiscard]] Align crossAlign() const noexcept {
    return mCrossAlign;
  }

  /** Distributes leftover main-axis space; `Start` keeps the historical left/top packing. */
  void setJustify(Justify justify);
  [[nodiscard]] Justify justify() const noexcept {
    return mJustify;
  }

  /**
   * Wraps items onto new lines instead of shrinking/overflowing a single line.
   *
   * A wrapped line gets the container's full main extent; lines are stacked on the cross axis and
   * separated by `lineGap` (which defaults to `gap` when left at 0).
   */
  void setWrap(bool wrap);
  [[nodiscard]] bool wraps() const noexcept {
    return mWrap;
  }
  void setLineGap(float gap);
  [[nodiscard]] float lineGap() const noexcept {
    return mLineGap;
  }

  Size measure(const Size& available) override;
  [[nodiscard]] Size intrinsicSize() const override;
  void layout() override;
  /** A flex is a boundary only when the parent pinned both dimensions; otherwise its size is content. */
  [[nodiscard]] bool isLayoutBoundary() const override {
    return hasFixedParentSize();
  }
  Size measureWithConstraints(const Constraints& constraints) override;

 private:
  Axis mAxis = Axis::Row;
  Insets mPadding{};
  float mGap = 0.0f;
  Align mCrossAlign = Align::Start;
  Justify mJustify = Justify::Start;
  bool mWrap = false;
  float mLineGap = 0.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_FLEX_H
