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

#ifndef CORAL_UI_RECONCILER_H
#define CORAL_UI_RECONCILER_H

#include <ui/Element.h>

#include <chrono>
#include <cstdint>
#include <memory>

namespace coral::ui {

class Layer;

/**
 * Turns immutable `Element` trees into mutable `Layer` trees and patches the difference.
 *
 * The first slice supports the closed element set in `Element.h` and stable keys. It is deliberately
 * React-shaped rather than a Compose slot table: the caller rebuilds a fresh tree, the reconciler
 * matches by `(kind, key)` and mutates the existing layers only where a prop changed or a child was
 * inserted/removed/moved.
 *
 * Thread affinity: UI thread. A reconciler owns one mounted tree; call `mount()` once and `update()`
 * for later state changes.
 */
class Reconciler {
 public:
  Reconciler();
  ~Reconciler();

  Reconciler(const Reconciler&) = delete;
  Reconciler& operator=(const Reconciler&) = delete;

  /** Mounts a fresh tree, replacing any previous root. */
  std::shared_ptr<Layer> mount(ElementPtr root);
  /** Duration of a `sharedElementTag` hero transition; 0 snaps instead of animating. */
  void setSharedElementDuration(std::chrono::milliseconds duration);
  [[nodiscard]] std::chrono::milliseconds sharedElementDuration() const noexcept;

  /**
   * Patches the mounted tree from @p root.
   *
   * If the root's kind/key differ, the whole tree is replaced. Otherwise text/color/stack props are
   * applied and children are matched by key (or by position when the key is empty).
   */
  void update(ElementPtr root);

  [[nodiscard]] std::shared_ptr<Layer> root() const;

  /**
   * How many nodes the last `update()` re-applied props to.
   *
   * This is the property-level invalidation counter: a rebuild whose values are unchanged must report
   * zero, and a single changed label exactly one. A node that owns callbacks is always re-applied
   * (the callbacks are not part of the value comparison), so panels built from such controls report
   * them even when nothing visibly changed.
   */
  [[nodiscard]] uint32_t lastAppliedNodeCount() const noexcept;
  [[nodiscard]] bool mounted() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> mImpl;
};

} // namespace coral::ui

#endif // CORAL_UI_RECONCILER_H
