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

#ifndef CORAL_UI_FRAMEPLAN_H
#define CORAL_UI_FRAMEPLAN_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <ui/Layer.h>

namespace graphics::gpu {
struct DrawList;
}

namespace coral::ui {

/**
 * One repaint boundary and the recorded lists that stand for it.
 *
 * The lists are the boundary's own recording, in paint order (its own paint first, then its children
 * left to right in paint order). They are immutable and borrowed: assembling a frame from them copies
 * nothing, and a later change replaces a list instead of mutating one the frame may still hold.
 */
struct FrameBoundary {
  Layer* boundary = nullptr;
  std::vector<std::shared_ptr<const graphics::gpu::DrawList>> lists;
};

/**
 * The boundary references a frame is assembled from.
 *
 * This is R3's frame in its smallest useful form: every repaint boundary in paint order, each with the
 * lists it already has, and nothing else. Non-boundary content is not part of the plan because the
 * caller records it once; the plan only says *which* boundary lists a frame is made of and in what
 * order they are painted.
 */
struct FramePlan {
  std::vector<FrameBoundary> boundaries;
  /** Lists the plan references. A frame built from the plan copies none of them. */
  std::size_t borrowedLists = 0;
  /** Bytes the borrowed lists hold, useful for the `listBytesLent` metric. */
  std::uint64_t borrowedBytes = 0;

  [[nodiscard]] bool empty() const noexcept {
    return boundaries.empty();
  }

  /** The boundaries in the order a frame paints them. */
  [[nodiscard]] std::vector<Layer*> order() const;
};

/**
 * Collects the boundary references under @p root, in paint order.
 *
 * The walk stops at a boundary: its recording already contains its subtree, so descending would paint
 * the contents twice. A boundary with no valid recording is still reported, with no lists, because the
 * caller has to record it before the frame is complete.
 */
[[nodiscard]] FramePlan collectFramePlan(const Layer& root);

} // namespace coral::ui

#endif // CORAL_UI_FRAMEPLAN_H
