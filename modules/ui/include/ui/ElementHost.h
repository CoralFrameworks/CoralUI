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

#ifndef CORAL_UI_ELEMENTHOST_H
#define CORAL_UI_ELEMENTHOST_H

#include <ui/Reconciler.h>
#include <ui/Window.h>

#include <core/observer/Observation.h>
#include <core/observer/Property.h>

#include <functional>
#include <memory>
#include <utility>

namespace coral::ui {

/**
 * Connects an immutable `Element` build function to a window and rebuilds it when a property changes.
 *
 * This is the missing piece between `Reconciler` (a manual mount/update) and a view model: the host
 * owns the mounted tree, coalesces every property change into one `post()`ed rebuild per UI turn, and
 * requests a frame after patching. It does not depend on `libs/mvvm`; a `mvvm::Property` is an
 * `observer::Property`, so a view model can bind directly.
 *
 * Threading: construct, `mount()`, `bind()` and destruction on the UI thread. The rebuild runs from
 * the window's posted-task queue, so several property writes in one turn produce one reconcile.
 */
class ElementHost {
 public:
  /** @param build pure function from current state to a fresh element tree */
  ElementHost(Window& window, std::function<ElementPtr()> build)
    : mWindow(window), mBuild(std::move(build)) {
  }

  ~ElementHost() {
    if (mOnUnmounted) mOnUnmounted();
    if (mAlive != nullptr) *mAlive = false;
  }

  ElementHost(const ElementHost&) = delete;
  ElementHost& operator=(const ElementHost&) = delete;

  /** Mounts the first tree; call once before setting the root on the window. */
  void mount() {
    mReconciler.mount(mBuild());
    mMounted = true;
    if (mOnMounted) mOnMounted();
  }

  /** Coalesces a rebuild request into one posted task. */
  void invalidate() {
    if (!mMounted || mPending) return;
    mPending = true;
    const std::shared_ptr<bool> alive = mAlive;
    mWindow.post([this, alive] {
      if (alive == nullptr || !*alive) return;
      mPending = false;
      mReconciler.update(mBuild());
      mWindow.requestFrame();
      if (mOnUpdated) mOnUpdated();
    });
  }

  /**
   * Component lifecycle hooks.
   *
   * `onMounted` runs after the first tree is mounted; `onUpdated` after every rebuild; `onUnmounted`
   * from the destructor, before queued rebuilds are invalidated. They are the place for focus/scroll
   * restoration or releasing resources owned by a component.
   */
  void setOnMounted(std::function<void()> callback) { mOnMounted = std::move(callback); }
  void setOnUpdated(std::function<void()> callback) { mOnUpdated = std::move(callback); }
  void setOnUnmounted(std::function<void()> callback) { mOnUnmounted = std::move(callback); }

  /** Subscribes a property to this host; keep the returned observation for as long as the binding. */
  template <class T>
  [[nodiscard]] observer::Observation bind(const observer::Property<T>& property) {
    return property.observe([this](const T&, const T&) { invalidate(); }, nullptr, false);
  }

  [[nodiscard]] Reconciler& reconciler() noexcept { return mReconciler; }
  /** The mounted root layer; null before `mount()`. */
  [[nodiscard]] std::shared_ptr<Layer> root() const { return mReconciler.root(); }
  /** Requests a rebuild without a property change (a command or an imperative edit). */
  void refresh() { invalidate(); }
  [[nodiscard]] bool mounted() const noexcept { return mMounted; }

 private:
  Window& mWindow;
  std::function<ElementPtr()> mBuild;
  Reconciler mReconciler;
  std::shared_ptr<bool> mAlive = std::make_shared<bool>(true);
  bool mMounted = false;
  bool mPending = false;
  std::function<void()> mOnMounted;
  std::function<void()> mOnUpdated;
  std::function<void()> mOnUnmounted;
};

} // namespace coral::ui

#endif // CORAL_UI_ELEMENTHOST_H
