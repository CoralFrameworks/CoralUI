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

#ifndef CORAL_UI_UNDO_H
#define CORAL_UI_UNDO_H

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

/**
 * One reversible edit.
 *
 * An action is recorded after the edit has already been applied: `undo` restores the previous state,
 * `redo` re-applies the new one. `mergeWith` lets a burst of small edits (typing, a continuous drag
 * that reports every pixel) collapse into one undo step; the default never merges.
 */
class UndoableAction {
 public:
  virtual ~UndoableAction() = default;

  virtual void undo() = 0;
  virtual void redo() = 0;

  /** Absorbs @p next into this action; @return true when it did (the stack then drops @p next). */
  virtual bool mergeWith(const UndoableAction& next) {
    (void) next;
    return false;
  }
  /** Label for a menu ("Undo Typing"). */
  [[nodiscard]] virtual std::string label() const {
    return {};
  }
};

/** The common case: a pair of closures. */
class FunctionUndoAction final : public UndoableAction {
 public:
  FunctionUndoAction(std::function<void()> undo, std::function<void()> redo, std::string label = {})
    : mUndo(std::move(undo)), mRedo(std::move(redo)), mLabel(std::move(label)) {}

  void undo() override {
    if (mUndo) mUndo();
  }
  void redo() override {
    if (mRedo) mRedo();
  }
  [[nodiscard]] std::string label() const override {
    return mLabel;
  }

 private:
  std::function<void()> mUndo;
  std::function<void()> mRedo;
  std::string mLabel;
};

/**
 * A bounded undo/redo history with transaction grouping.
 *
 * Transactions matter because most edits are composite: dragging a slider changes a value many
 * times, and each change must not become a separate undo step. `beginTransaction`/`commitTransaction`
 * collect the actions pushed in between into one composite step (undo runs them in reverse order).
 * While a transaction is open, `apply()` runs the action but records it in the pending group.
 *
 * The stack is UI-thread state. `setChangedCallback` lets a menu or toolbar refresh after anything
 * that affects availability or labels.
 */
class UndoStack {
 public:
  explicit UndoStack(std::size_t limit = 200);

  /** Records an action that has already been applied. Clears the redo history. */
  bool push(std::unique_ptr<UndoableAction> action);

  /** Runs `action->redo()` and records it; the convenience for "apply through the stack". */
  bool apply(std::unique_ptr<UndoableAction> action);

  [[nodiscard]] bool canUndo() const noexcept {
    return !mUndo.empty();
  }
  [[nodiscard]] bool canRedo() const noexcept {
    return !mRedo.empty();
  }
  bool undo();
  bool redo();
  void clear();

  [[nodiscard]] std::size_t undoCount() const noexcept {
    return mUndo.size();
  }
  [[nodiscard]] std::size_t redoCount() const noexcept {
    return mRedo.size();
  }
  [[nodiscard]] std::size_t limit() const noexcept {
    return mLimit;
  }
  void setLimit(std::size_t limit);

  [[nodiscard]] std::string undoLabel() const;
  [[nodiscard]] std::string redoLabel() const;

  /**
   * Starts grouping edits into one step.
   *
   * A nested begin is ignored (returns false) so a control inside a drag cannot create a second
   * group. Commit with no recorded edits is a no-op and returns false.
   */
  bool beginTransaction(std::string label = {});
  bool commitTransaction();
  /** Drops the pending group without recording it (the caller already reverted the state). */
  void abortTransaction();
  [[nodiscard]] bool inTransaction() const noexcept {
    return mTransactionOpen;
  }

  void setChangedCallback(std::function<void()> callback) {
    mChanged = std::move(callback);
  }

 private:
  void trim();
  void notifyChanged() const {
    if (mChanged) mChanged();
  }

  std::vector<std::unique_ptr<UndoableAction>> mUndo;
  std::vector<std::unique_ptr<UndoableAction>> mRedo;
  /** Edits collected while a transaction is open; committed as one composite action. */
  std::vector<std::unique_ptr<UndoableAction>> mPending;
  std::size_t mLimit;
  bool mTransactionOpen = false;
  std::string mTransactionLabel;
  std::function<void()> mChanged;
};

} // namespace coral::ui

#endif // CORAL_UI_UNDO_H
