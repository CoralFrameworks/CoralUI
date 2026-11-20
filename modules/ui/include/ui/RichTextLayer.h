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

#ifndef CORAL_UI_RICHTEXTLAYER_H
#define CORAL_UI_RICHTEXTLAYER_H

#include <ui/Types.h>

#include <ui/Layer.h>
#include <ui/RichText.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace coral::ui {

class TextPainter;

/** A multi-line, multi-style text layer. `TextLayer` is the single-style shortcut over this layout. */
class RichTextLayer final : public Layer {
 public:
  explicit RichTextLayer(std::vector<RichTextRun> runs = {});
  ~RichTextLayer() override;

  void setRuns(std::vector<RichTextRun> runs);
  [[nodiscard]] const std::vector<RichTextRun>& runs() const noexcept {
    return mRuns;
  }

  void setWrapWidth(float width);
  [[nodiscard]] float wrapWidth() const noexcept {
    return mWrapWidth;
  }

  /** Optional CSS-like line-break policy (language, word-break, hyphenation, ...). */
  void setLineBreakPolicy(const LineBreakPolicy& policy);
  [[nodiscard]] const LineBreakPolicy& lineBreakPolicy() const noexcept {
    return mPolicy;
  }

  /** Tab advance in space widths; 0 leaves U+0009 untouched. */
  void setTabSize(uint32_t spaces);
  [[nodiscard]] uint32_t tabSize() const noexcept { return mTabSize; }

  void applyTheme(const Theme& theme) override;

  /** Attach the platform fallback chain to every run that has a font. */
  void setSystemFontFallback(bool enabled);
  [[nodiscard]] bool usesSystemFontFallback() const noexcept {
    return mSystemFallback;
  }

  /** Fixed line box height; 0 uses the largest font metrics on each line. */
  void setLineHeight(float height);
  [[nodiscard]] float lineHeight() const noexcept {
    return mLineHeight;
  }
  void setTextAlign(TextAlign align);
  [[nodiscard]] TextAlign textAlign() const noexcept {
    return mAlign;
  }
  void setOrientation(TextOrientation orientation);
  [[nodiscard]] TextOrientation orientation() const noexcept {
    return mOrientation;
  }
  void setDirection(TextDirectionMode direction);
  [[nodiscard]] TextDirectionMode direction() const noexcept {
    return mDirection;
  }

  /** 0 means unlimited; a positive value clamps the line count. */
  void setMaxLines(int lines);
  [[nodiscard]] int maxLines() const noexcept {
    return mMaxLines;
  }
  /** Append an ellipsis when maxLines truncates the text. */
  void setEllipsis(bool ellipsis);
  [[nodiscard]] bool ellipsis() const noexcept {
    return mEllipsis;
  }

  void setSelectable(bool selectable);
  [[nodiscard]] bool selectable() const noexcept {
    return mSelectable;
  }
  void setSelection(size_t start, size_t end);
  void clearSelection();
  [[nodiscard]] size_t selectionStart() const noexcept {
    return std::min(mSelectionStart, mSelectionEnd);
  }
  [[nodiscard]] size_t selectionEnd() const noexcept {
    return std::max(mSelectionStart, mSelectionEnd);
  }
  [[nodiscard]] bool hasSelection() const noexcept {
    return selectionStart() != selectionEnd();
  }
  [[nodiscard]] size_t indexAt(const Point& point) const;
  void setSelectionColor(Color color);

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  /** Last laid-out result (valid after measure/layout). */
  [[nodiscard]] const RichTextLayout& layout() const noexcept {
    return mLayout;
  }
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  [[nodiscard]] bool focusable() const override {
    return mSelectable;
  }
  [[nodiscard]] bool capturesPointerOnPress() const override {
    return mSelectable;
  }

 private:
  [[nodiscard]] std::vector<RichTextRun> layoutRuns() const;
  [[nodiscard]] RichTextLayoutOptions layoutOptions(float wrapWidth) const;
  std::vector<RichTextRun> mRuns;
  RichTextLayout mLayout;
  std::vector<std::vector<std::unique_ptr<TextPainter>>> mPainters;
  std::unique_ptr<TextPainter> mHyphenPainter;
  bool mSystemFallback = false;
  float mLineHeight = 0.0f;
  TextAlign mAlign = TextAlign::Left;
  TextOrientation mOrientation = TextOrientation::Horizontal;
  TextDirectionMode mDirection = TextDirectionMode::Auto;
  int mMaxLines = 0;
  bool mEllipsis = false;
  float mWrapWidth = 0.0f;
  LineBreakPolicy mPolicy;
  bool mPolicyExplicit = false;
  uint32_t mTabSize = 8;
  bool mSelectable = false;
  bool mSelecting = false;
  size_t mSelectionStart = 0;
  size_t mSelectionEnd = 0;
  size_t mSelectionAnchor = 0;
  Color mSelectionColor = Color::MakeRGBA8(86, 156, 255, 90);
};

} // namespace coral::ui

#endif // CORAL_UI_RICHTEXTLAYER_H
