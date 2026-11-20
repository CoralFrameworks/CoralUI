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

#ifndef CORAL_MVVM_OBSERVABLELIST_H
#define CORAL_MVVM_OBSERVABLELIST_H

#include <core/observer/Event.h>

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace coral::mvvm {

/**
 * A list whose structural changes are observable.
 *
 * The API is deliberately index based, because that is what a keyed list view needs: a removal
 * reports the range it removed, a move reports the final index, and `setAll` reports a reset. The
 * item values themselves are not copied into the event; a consumer that needs them calls `items()`
 * (or `at()`) after the notification, which is also what makes the event cheap for large lists.
 *
 * `T` must be copy-constructible because `items()` returns a snapshot. The list is internally
 * synchronized, so a worker may produce items while the UI observes them; callbacks still run on the
 * mutating thread unless the consumer binds an executor/dispatcher.
 */
template <class T>
class ObservableList {
 public:
  struct Inserted {
    std::size_t index = 0;
    std::size_t count = 0;
  };
  struct Removed {
    std::size_t index = 0;
    std::size_t count = 0;
  };
  struct Moved {
    std::size_t from = 0;
    std::size_t to = 0;
  };
  struct Replaced {
    std::size_t index = 0;
    std::size_t count = 0;
  };
  struct Reset {};

  ObservableList() = default;
  explicit ObservableList(std::vector<T> items) : mItems(std::move(items)) {}

  ObservableList(const ObservableList&) = delete;
  ObservableList& operator=(const ObservableList&) = delete;
  ObservableList(ObservableList&&) = delete;
  ObservableList& operator=(ObservableList&&) = delete;

  [[nodiscard]] std::size_t size() const {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mItems.size();
  }

  [[nodiscard]] bool empty() const {
    return size() == 0;
  }

  /** A copy of the current sequence; safe to keep after the call. */
  [[nodiscard]] std::vector<T> items() const {
    const std::lock_guard<std::mutex> lock(mMutex);
    return mItems;
  }

  /** A copy of one item, or null when @p index is out of range. */
  [[nodiscard]] std::optional<T> at(const std::size_t index) const {
    const std::lock_guard<std::mutex> lock(mMutex);
    if (index >= mItems.size()) return std::nullopt;
    return mItems[index];
  }

  void reserve(const std::size_t capacity) {
    const std::lock_guard<std::mutex> lock(mMutex);
    mItems.reserve(capacity);
  }

  void push_back(T value) {
    insert(size(), std::move(value));
  }

  /** Inserts at @p index (clamped to size) and emits `Inserted{index, 1}`. */
  void insert(std::size_t index, T value) {
    {
      const std::lock_guard<std::mutex> lock(mMutex);
      if (index > mItems.size()) index = mItems.size();
      mItems.insert(mItems.begin() + static_cast<std::ptrdiff_t>(index), std::move(value));
    }
    mInserted.emit(Inserted{ index, 1 });
  }

  /** Erases up to @p count items starting at @p index; emits one `Removed` for the actual range. */
  void erase(std::size_t index, std::size_t count = 1) {
    {
      const std::lock_guard<std::mutex> lock(mMutex);
      if (index >= mItems.size() || count == 0) return;
      count = std::min(count, mItems.size() - index);
      mItems.erase(mItems.begin() + static_cast<std::ptrdiff_t>(index),
        mItems.begin() + static_cast<std::ptrdiff_t>(index + count));
    }
    mRemoved.emit(Removed{ index, count });
  }

  void pop_back() {
    const auto current = size();
    if (current > 0) erase(current - 1);
  }

  /** Replaces one item; emits `Replaced{index, 1}`. Out-of-range indices are ignored. */
  void replace(const std::size_t index, T value) {
    {
      const std::lock_guard<std::mutex> lock(mMutex);
      if (index >= mItems.size()) return;
      mItems[index] = std::move(value);
    }
    mReplaced.emit(Replaced{ index, 1 });
  }

  /** Moves the item at @p from to @p to (the final index); emits `Moved`. */
  void move(const std::size_t from, const std::size_t to) {
    {
      const std::lock_guard<std::mutex> lock(mMutex);
      if (from >= mItems.size() || to >= mItems.size() || from == to) return;
      T value = std::move(mItems[from]);
      mItems.erase(mItems.begin() + static_cast<std::ptrdiff_t>(from));
      mItems.insert(mItems.begin() + static_cast<std::ptrdiff_t>(to), std::move(value));
    }
    mMoved.emit(Moved{ from, to });
  }

  /** Replaces the whole sequence and emits `Reset`. */
  void setAll(std::vector<T> items) {
    {
      const std::lock_guard<std::mutex> lock(mMutex);
      mItems = std::move(items);
    }
    mReset.emit(Reset{});
  }

  void clear() {
    {
      const std::lock_guard<std::mutex> lock(mMutex);
      if (mItems.empty()) return;
      mItems.clear();
    }
    mReset.emit(Reset{});
  }

  [[nodiscard]] observer::Observation observeInserted(std::function<void(const Inserted&)> observer,
      std::shared_ptr<observer::Executor> executor = nullptr) const {
    return mInserted.observe(std::move(observer), std::move(executor));
  }

  [[nodiscard]] observer::Observation observeRemoved(std::function<void(const Removed&)> observer,
      std::shared_ptr<observer::Executor> executor = nullptr) const {
    return mRemoved.observe(std::move(observer), std::move(executor));
  }

  [[nodiscard]] observer::Observation observeMoved(std::function<void(const Moved&)> observer,
      std::shared_ptr<observer::Executor> executor = nullptr) const {
    return mMoved.observe(std::move(observer), std::move(executor));
  }

  [[nodiscard]] observer::Observation observeReplaced(std::function<void(const Replaced&)> observer,
      std::shared_ptr<observer::Executor> executor = nullptr) const {
    return mReplaced.observe(std::move(observer), std::move(executor));
  }

  [[nodiscard]] observer::Observation observeReset(std::function<void(const Reset&)> observer,
      std::shared_ptr<observer::Executor> executor = nullptr) const {
    return mReset.observe(std::move(observer), std::move(executor));
  }

 private:
  mutable std::mutex mMutex;
  std::vector<T> mItems;
  mutable observer::Event<Inserted> mInserted;
  mutable observer::Event<Removed> mRemoved;
  mutable observer::Event<Moved> mMoved;
  mutable observer::Event<Replaced> mReplaced;
  mutable observer::Event<Reset> mReset;
};

} // namespace coral::mvvm

#endif // CORAL_MVVM_OBSERVABLELIST_H
