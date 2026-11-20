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

#include <ui/FramePlan.h>
#include <gpu/DrawList.h>

#include <algorithm>

namespace coral::ui {

namespace {

/** A cheap size estimate; the exact accounting lives with the recording cache. */
std::uint64_t listBytes(const graphics::gpu::DrawList& list) {
  std::uint64_t bytes = 0;
  bytes += list.commands.size() * sizeof(graphics::gpu::DrawList::Command);
  bytes += list.contours.size() * sizeof(graphics::gpu::DrawList::Contour);
  bytes += list.points.size() * sizeof(graphics::Point);
  return bytes;
}

void collect(const Layer& layer, FramePlan& plan) {
  for (Layer* child: layer.paintOrderedChildren()) {
    if (child == nullptr || !child->visible()) continue;
    if (!child->isRepaintBoundary()) {
      // A boundary's recording already contains its subtree; descending into it would paint its
      // contents twice. Everything else is walked so nested boundaries are found in paint order.
      collect(*child, plan);
      continue;
    }
    FrameBoundary entry;
    entry.boundary = child;
    entry.lists = child->borrowRecording();
    for (const std::shared_ptr<const graphics::gpu::DrawList>& list: entry.lists) {
      if (list == nullptr) continue;
      ++plan.borrowedLists;
      plan.borrowedBytes += listBytes(*list);
    }
    plan.boundaries.push_back(std::move(entry));
  }
}

} // namespace

std::vector<Layer*> FramePlan::order() const {
  std::vector<Layer*> ordered;
  ordered.reserve(boundaries.size());
  for (const FrameBoundary& entry: boundaries) ordered.push_back(entry.boundary);
  return ordered;
}

FramePlan collectFramePlan(const Layer& root) {
  FramePlan plan;
  collect(root, plan);
  return plan;
}

} // namespace coral::ui
