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

#ifndef CORAL_UI_REF_H
#define CORAL_UI_REF_H

#include <memory>
#include <utility>

namespace coral::ui {

class Layer;

/**
 * A handle to a mounted layer.
 *
 * A component cannot keep a raw pointer into the layer tree: reconciliation may replace the layer on
 * the next build. A `LayerRef` is the stable handle instead: attach it to an element through
 * `ElementProps::ref`, the reconciler fills it in when the layer mounts, and a component reads
 * `layer()` when it needs to scroll, focus or measure. The weak pointer means the ref never keeps a
 * detached layer alive; it simply expires and `valid()` turns false.
 *
 * The ref does not transfer ownership: the layer tree owns the layer.
 */
class LayerRef {
 public:
  LayerRef() = default;

  [[nodiscard]] std::shared_ptr<Layer> layer() const {
    return mLayer.lock();
  }
  void set(std::shared_ptr<Layer> layer) {
    mLayer = std::move(layer);
  }
  void clear() {
    mLayer.reset();
  }
  [[nodiscard]] bool valid() const noexcept {
    return !mLayer.expired();
  }

 private:
  std::weak_ptr<Layer> mLayer;
};

} // namespace coral::ui

#endif // CORAL_UI_REF_H
