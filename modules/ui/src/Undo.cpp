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

#include <ui/Undo.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {

/** A transaction: undoes its children in reverse order, redoes them forward. */
class CompositeUndoAction final : public UndoableAction {
 public:
  CompositeUndoAction(std::vector<std::unique_ptr<UndoableAction>> actions, std::string label)
    : mActions(std::move(actions)), mLabel(std::move(label)) {}

  void undo() override {
    for (auto position = mActions.rbegin(); position != mActions.rend(); ++position) {
      if (*position) (*position)->undo();
    }
  }
  void redo() override {
    for (auto& action: mActions) {
      if (action) action->redo();
    }
  }
  [[nodiscard]] std::string label() const override {
    return mLabel;
  }

 private:
  std::vector<std::unique_ptr<UndoableAction>> mActions;
  std::string mLabel;
};

} // namespace

UndoStack::UndoStack(const std::size_t limit) : mLimit(std::max<std::size_t>(1, limit)) {}

void UndoStack::setLimit(const std::size_t limit) {
  mLimit = std::max<std::size_t>(1, limit);
  trim();
  notifyChanged();
}

bool UndoStack::push(std::unique_ptr<UndoableAction> action) {
  if (action == nullptr) return false;
  if (mTransactionOpen) {
    mPending.push_back(std::move(action));
    notifyChanged();
    return true;
  }
  // A new edit invalidates the redo branch.
  mRedo.clear();
  mUndo.push_back(std::move(action));
  trim();
  notifyChanged();
  return true;
}

bool UndoStack::apply(std::unique_ptr<UndoableAction> action) {
  if (action == nullptr) return false;
  action->redo();
  return push(std::move(action));
}

bool UndoStack::undo() {
  if (mUndo.empty()) return false;
  // An undo while a transaction is open would break the grouping invariant; commit it first.
  if (mTransactionOpen) commitTransaction();
  std::unique_ptr<UndoableAction> action = std::move(mUndo.back());
  mUndo.pop_back();
  action->undo();
  mRedo.push_back(std::move(action));
  trim();
  notifyChanged();
  return true;
}

bool UndoStack::redo() {
  if (mRedo.empty()) return false;
  std::unique_ptr<UndoableAction> action = std::move(mRedo.back());
  mRedo.pop_back();
  action->redo();
  mUndo.push_back(std::move(action));
  trim();
  notifyChanged();
  return true;
}

void UndoStack::clear() {
  if (mUndo.empty() && mRedo.empty() && mPending.empty()) return;
  mUndo.clear();
  mRedo.clear();
  mPending.clear();
  mTransactionOpen = false;
  notifyChanged();
}

std::string UndoStack::undoLabel() const {
  return mUndo.empty() ? std::string() : mUndo.back()->label();
}

std::string UndoStack::redoLabel() const {
  return mRedo.empty() ? std::string() : mRedo.back()->label();
}

bool UndoStack::beginTransaction(std::string label) {
  if (mTransactionOpen) return false;
  mTransactionOpen = true;
  mTransactionLabel = std::move(label);
  mPending.clear();
  return true;
}

bool UndoStack::commitTransaction() {
  if (!mTransactionOpen) return false;
  mTransactionOpen = false;
  if (mPending.empty()) {
    mTransactionLabel.clear();
    return false;
  }
  std::vector<std::unique_ptr<UndoableAction>> actions = std::move(mPending);
  mPending.clear();
  std::string label = mTransactionLabel;
  mTransactionLabel.clear();
  mRedo.clear();
  mUndo.push_back(std::make_unique<CompositeUndoAction>(std::move(actions), std::move(label)));
  trim();
  notifyChanged();
  return true;
}

void UndoStack::abortTransaction() {
  if (!mTransactionOpen) return;
  mTransactionOpen = false;
  mPending.clear();
  mTransactionLabel.clear();
  notifyChanged();
}

void UndoStack::trim() {
  while (mUndo.size() > mLimit) mUndo.erase(mUndo.begin());
}

} // namespace coral::ui
