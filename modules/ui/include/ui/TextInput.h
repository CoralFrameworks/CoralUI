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

#ifndef CORAL_UI_TEXTINPUT_H
#define CORAL_UI_TEXTINPUT_H

#include <ui/Types.h>

#include <ui/Layer.h>
#include <ui/Undo.h>
#include <ui/SpellCheck.h>
#include <ui/TextLayout.h>


#include <core/observer/Event.h>
#include <core/observer/Observation.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace graphics {
}

namespace coral::ui {

struct Theme;
class Menu;
class TextPainter;

/**
 * A single-line editable text field.
 *
 * It takes focus like any other layer, consumes characters through `onTextInput` and edits around a
 * UTF-8 byte cursor. This is the direct/ASCII input path the first editor milestone needs; a platform
 * IME adds composition ranges in front of the same insertion point later.
 */
class TextInput final : public Layer {
 public:
  TextInput(std::string text, std::shared_ptr<const Font> font);
  ~TextInput() override;

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }

  void setFont(std::shared_ptr<const Font> font);
  void setFontFallback(std::shared_ptr<const FontFallback> fallback);
  [[nodiscard]] const std::shared_ptr<const FontFallback>& fontFallback() const noexcept {
    return mFallback;
  }
  /** Build the fallback chain from the platform font directories (see ui/SystemFonts.h). */
  void setSystemFontFallback(bool enabled);
  [[nodiscard]] bool usesSystemFontFallback() const noexcept {
    return mSystemFallback;
  }
  /** Optional CSS-like policy; defaults to `overflow-wrap: break-word` for the editor. */
  void setLineBreakPolicy(const LineBreakPolicy& policy);
  [[nodiscard]] const LineBreakPolicy& lineBreakPolicy() const noexcept {
    return mPolicy;
  }
  void setColors(Color background, Color foreground);
  void setPlaceholder(std::string placeholder);
  /**
   * Controls whether the field paints its own background/border.
   *
   * Composite fields (NumberField, SearchField) embed a TextInput and draw one border around the
   * whole control, so the inner field turns this off.
   */
  /**
   * Enables platform spell checking and paints a red underline under each misspelled byte range.
   * Runs synchronously on text changes; a large document should use `spellCheck()` off the UI thread
   * and set `spellIssues` through the app, which this convenience API does not model yet.
   */
  void setSpellCheckEnabled(bool enabled);
  [[nodiscard]] bool spellCheckEnabled() const noexcept {
    return mSpellCheckEnabled;
  }
  [[nodiscard]] const std::vector<SpellIssue>& spellIssues() const noexcept {
    return mSpellIssues;
  }

  void setBordered(bool bordered) {
    mBordered = bordered;
    setNeedsPaint();
  }
  /**
   * Controls whether the field itself can take focus.
   *
   * An embedded field is driven by its parent; the parent owns the focus and forwards text events,
   * while the inner field still handles pointer selection and painting.
   */
  void setFocusable(bool focusable) {
    mFocusable = focusable;
  }
  /** Subscribes to text changes; the returned observation unsubscribes on destruction. */
  [[nodiscard]] observer::Observation observeChanged(
    std::function<void(const std::string&)> callback) const;

  void setPreferredWidth(float width);
  /** Multi-line editing: Enter inserts a newline, Up/Down move between visual lines. */
  void setMultiline(bool multiline);
  [[nodiscard]] bool multiline() const noexcept {
    return mMultiline;
  }

  [[nodiscard]] bool focusable() const override {
    return mFocusable;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  [[nodiscard]] bool capturesPointerOnPress() const override {
    return true;
  }
  bool onKeyDown(const KeyEvent& event) override;
  bool onTextInput(char32_t character) override;
  bool onTextComposition(const std::string& text) override;
  bool onTextCommit(const std::string& text) override;
  bool onTextDelete(int32_t index, uint32_t length) override;
  [[nodiscard]] bool acceptsTextInput() const override {
    return true;
  }
  [[nodiscard]] Rect imeCursorRect() const override;
  void onFocusChanged(bool focused) override;

  /** The IME composition shown before it is committed. */
  [[nodiscard]] const std::string& composition() const noexcept {
    return mComposition;
  }

  /** The byte offset of the active cursor. */
  /**
   * Caret blink interval; 0 disables blinking (the caret stays solid).
   *
   * The phase restarts on focus, cursor moves and caret edits, so the caret is always visible right
   * after the user acts instead of possibly starting hidden.
   */
  void setCaretBlinkInterval(std::chrono::milliseconds interval);
  [[nodiscard]] std::chrono::milliseconds caretBlinkInterval() const noexcept {
    return mCaretBlinkInterval;
  }
  /** Visible phase at @p now; composition (IME preedit) suppresses blinking. */
  [[nodiscard]] bool caretVisibleAt(std::chrono::steady_clock::time_point now) const;
  void onFrame(std::chrono::steady_clock::time_point now) override;

  [[nodiscard]] size_t cursor() const noexcept {
    return mCursor;
  }
  [[nodiscard]] bool hasSelection() const noexcept {
    return mSelectionAnchor != mCursor;
  }
  [[nodiscard]] size_t selectionStart() const noexcept {
    return std::min(mSelectionAnchor, mCursor);
  }
  /** The selection anchor (the end that stays put while the caret moves). */
  [[nodiscard]] size_t selectionAnchor() const noexcept {
    return mSelectionAnchor;
  }
  [[nodiscard]] size_t selectionEnd() const noexcept {
    return std::max(mSelectionAnchor, mCursor);
  }
  [[nodiscard]] std::string selectedText() const;
  /** Vertical content offset for a constrained multi-line field. */
  [[nodiscard]] float scrollOffset() const noexcept {
    return mScrollY;
  }
  void setCursor(size_t cursor);
  void setSelection(size_t anchor, size_t cursor);
  void selectAll();

  /** Applies the edit history of the field; returns false when there is nothing to undo/redo. */
  /**
   * Shares this field's edit history with other fields; nullptr returns to the private history.
   * The recorded actions reference the field, so clear the shared stack before destroying the fields
   * (or give the stack the same lifetime as the editor that owns them).
   */
  void setUndoStack(std::shared_ptr<UndoStack> stack);
  [[nodiscard]] const std::shared_ptr<UndoStack>& undoStack() const noexcept {
    return mSharedUndo;
  }
  bool undo();
  bool redo();

 protected:
  void applyTheme(const Theme& theme) override;

  /** The selection in UTF-16 code units, which is what UI Automation addresses text with. */
  [[nodiscard]] std::optional<AccessibleTextSelection> accessibleTextSelection() const override;
  /**
   * `SetValue` replaces the text; `Select` with `"<start>,<end>"` (UTF-16 units) moves the selection.
   * Text range operations go through these two, so a bridge never reaches into the control's buffers.
   */
  bool performAccessibilityAction(AccessibilityAction action, std::string_view value) override;

 private:
  void insert(char32_t character);
  void eraseBefore();
  void eraseAfter();
  void replaceSelection(const std::string& text);
  void recordEdit(size_t start, size_t oldEnd, size_t newEnd);
  void eraseSelection();
  void copySelection() const;
  void cutSelection();
  void paste();
  /** Publishes the marked text to the PRIMARY selection ("select to copy"). */
  void publishPrimarySelection();
  /** Middle-click paste: inserts the PRIMARY selection at the cursor. */
  void pastePrimarySelection();
  void ensureContextMenu();
  void showContextMenu(const Point& position);
  void updateContextMenu();
  void replaceContextWord(const std::string& word);
  [[nodiscard]] std::pair<size_t, size_t> wordRangeAt(size_t index) const;
  [[nodiscard]] std::pair<size_t, size_t> lineRangeAt(size_t index) const;
  void eraseWordBefore();
  void eraseWordAfter();
  void ensureCursorVisible();
  void scrollBy(float delta);
  void pushUndo();
  void moveCursor(size_t cursor, bool extend);
  [[nodiscard]] size_t indexAtX(float x) const;
  [[nodiscard]] size_t indexAtPoint(const Point& point) const;
  [[nodiscard]] float xForIndex(size_t index) const;
  [[nodiscard]] float xForIndexInLine(size_t index, const TextLine& line) const;
  void drawLine(RenderContext& context, const TextLine& line, float left, float baseline,
    const Paint& paint, size_t painterIndex);
  void drawSelectionForLine(RenderContext& context, const TextLine& line, float left, float top,
    float height, size_t begin, size_t end);
  void drawSpellIssuesForLine(RenderContext& context, const TextLine& line, float left, float top,
    float height);
  void refreshSpellIssues();
  [[nodiscard]] size_t lineIndexForCursor(size_t cursor) const;
  [[nodiscard]] size_t lineStart(int line) const;
  [[nodiscard]] size_t lineEnd(int line) const;
  /** @return the next caret byte offset in visual order (Left/Right arrow). */
  [[nodiscard]] size_t visualCaretNeighbor(bool left) const;
  void ensureLayout(float width) const;
  [[nodiscard]] float lineHeight() const;
  void notifyChanged();

  std::string mText;
  std::string mPlaceholder;
  std::string mComposition;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<const FontFallback> mFallback;
  bool mSystemFallback = false;
  LineBreakPolicy mPolicy;
  bool mSpellCheckEnabled = false;
  std::vector<SpellIssue> mSpellIssues;
  bool mPolicyExplicit = false;
  Color mBackground = Color::MakeRGBA8(24, 26, 32, 255);
  Color mBorder = Color::MakeRGBA8(62, 67, 78, 255);
  Color mFocus = Color::MakeRGBA8(86, 156, 255, 255);
  Color mForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Color mMuted = Color::MakeRGBA8(148, 163, 184, 255);
  float mPadding = 10.0f;
  float mRadius = 4.0f;
  bool mMultiline = false;
  mutable TextLayout mLayout;
  mutable float mLayoutWidth = -1.0f;
  mutable std::optional<TextEditRange> mPendingEdit;
  mutable observer::Event<std::string> mChanged;
  size_t mCursor = 0;
  size_t mSelectionAnchor = 0;
  std::shared_ptr<UndoStack> mSharedUndo;
  std::vector<std::string> mUndo;
  std::vector<std::string> mRedo;
  std::shared_ptr<Menu> mContextMenu;
  std::size_t mContextWordBegin = 0;
  std::size_t mContextWordEnd = 0;
  int mContextCutIndex = 0;
  int mContextCopyIndex = 1;
  int mContextPasteIndex = 2;
  std::chrono::steady_clock::time_point mLastClick{};
  Point mLastClickPosition{};
  int mClickCount = 0;
  bool mSelecting = false;
  /** The text a press inside the current selection would drag out; empty when the press was not one. */
  std::string mDragOutText;
  float mScrollY = 0.0f;
  float mPreferredWidth = 220.0f;
  bool mFocused = false;
  std::chrono::milliseconds mCaretBlinkInterval{ 530 };
  std::chrono::steady_clock::time_point mCaretBlinkStart{};
  bool mCaretVisibleCached = true;
  bool mBordered = true;
  bool mFocusable = true;
  std::unique_ptr<TextPainter> mTextPainter;
  std::vector<std::vector<std::unique_ptr<TextPainter>>> mRunPainters;
  std::unique_ptr<TextPainter> mCompositionPainter;
  std::unique_ptr<TextPainter> mPlaceholderPainter;
  std::unique_ptr<TextPainter> mHyphenPainter;
};

} // namespace coral::ui

#endif // CORAL_UI_TEXTINPUT_H
