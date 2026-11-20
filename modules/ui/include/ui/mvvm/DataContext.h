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

#ifndef CORAL_UI_MVVM_DATACONTEXT_H
#define CORAL_UI_MVVM_DATACONTEXT_H

#include <algorithm>
#include <memory>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace coral::ui::mvvm {

/**
 * A scoped, typed view-model container.
 *
 * A page or component owns the root context; a nested control can push a more specific view model
 * and pop it when it leaves. Lookup walks from the newest scope outward, so a child can shadow a
 * parent's view model without the parent knowing.
 *
 * The context stores `shared_ptr<void>` plus a type token and performs no casting: `get<T>()` only
 * returns an object that was stored as exactly `T`. That keeps the dependency direction clean (the
 * UI layer does not need RTTI or a common base class for view models).
 *
 * Thread affinity: the UI thread. Move a context between pages only while neither side is reading.
 */
class DataContext {
 public:
  DataContext() = default;

  /** Stores @p value in the current scope, replacing a value of the same type. */
  template <class T>
  void set(std::shared_ptr<T> value) {
    const std::type_index type(typeid(T));
    for (Scope& scope: mScopes) {
      auto it = std::find_if(scope.values.begin(), scope.values.end(),
        [type](const Entry& entry) { return entry.type == type; });
      if (it != scope.values.end()) {
        it->value = std::move(value);
        return;
      }
    }
    mScopes.back().values.push_back(Entry{ type, std::move(value) });
  }

  /** Finds a value by exact type in the newest scope that has one, or null. */
  template <class T>
  [[nodiscard]] std::shared_ptr<T> get() const {
    const std::type_index type(typeid(T));
    for (auto scope = mScopes.rbegin(); scope != mScopes.rend(); ++scope) {
      for (const Entry& entry: scope->values) {
        if (entry.type != type) continue;
        return std::static_pointer_cast<T>(entry.value);
      }
    }
    return nullptr;
  }

  /** Pushes a nested scope; pair with `pop()`. */
  void push() {
    mScopes.emplace_back();
  }

  /** Pops the newest scope; the root scope is never removed. */
  void pop() {
    if (mScopes.size() > 1) mScopes.pop_back();
  }

  [[nodiscard]] bool hasValue() const noexcept {
    return !mScopes.empty();
  }

  [[nodiscard]] size_t depth() const noexcept {
    return mScopes.size();
  }

  void clear() {
    mScopes.clear();
    mScopes.emplace_back();
  }

  /** RAII nested scope. */
  class ScopeGuard {
   public:
    explicit ScopeGuard(DataContext& context) : mContext(&context) {
      mContext->push();
    }
    ~ScopeGuard() {
      if (mContext != nullptr) mContext->pop();
    }
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&& other) noexcept : mContext(other.mContext) {
      other.mContext = nullptr;
    }

   private:
    DataContext* mContext = nullptr;
  };

 private:
  struct Entry {
    std::type_index type;
    std::shared_ptr<void> value;
  };
  struct Scope {
    std::vector<Entry> values;
  };
  std::vector<Scope> mScopes { Scope{} };
};

} // namespace coral::ui::mvvm

#endif // CORAL_UI_MVVM_DATACONTEXT_H
