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

#ifndef CORAL_UI_TEXTLAYER_H
#define CORAL_UI_TEXTLAYER_H

#include <ui/Types.h>

#include <ui/Layer.h>
#include <ui/TextLayout.h>


#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace graphics {
class Bitmap;
}

namespace coral::ui {

class TextPainter;

/**
 * A single line of text.
 *
 * `measure` comes from the font's own metrics, so a row can lay a label out before it is ever drawn.
 * Wrapping and rich runs are the job of the TextLayout milestone; this is the leaf that milestone will
 * produce, not a second text engine.
 */
class TextLayer final : public Layer {
 public:
  TextLayer(std::string text, std::shared_ptr<const Font> font,
    Color color = Color::White());
  ~TextLayer() override;

  void setText(std::string text);
  [[nodiscard]] const std::string& text() const noexcept {
    return mText;
  }

  void setFont(std::shared_ptr<const Font> font);
  [[nodiscard]] const std::shared_ptr<const Font>& font() const noexcept {
    return mFont;
  }
  /** Optional per-character fallback faces; layout measures each run in its own face. */
  void setFontFallback(std::shared_ptr<const FontFallback> fallback);
  [[nodiscard]] const std::shared_ptr<const FontFallback>& fontFallback() const noexcept {
    return mFallback;
  }
  /** Build the fallback chain from the platform font directories (see ui/SystemFonts.h). */
  void setSystemFontFallback(bool enabled);
  [[nodiscard]] bool usesSystemFontFallback() const noexcept {
    return mSystemFallback;
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

  void setColor(Color color);
  [[nodiscard]] Color color() const noexcept {
    return mColor;
  }

  /**
   * Tell the text painter the solid colour underneath this line, when the layer owner knows it.
   *
   * This enables per-channel linear-light subpixel compositing against that backdrop. Without it the
   * painter falls back to the single-alpha light-on-dark approximation; arbitrary images/gradients
   * still need a layer-level linear-light pass.
   */
  void setTextBackgroundColor(Color color);
  void clearTextBackgroundColor();
  /**
   * Declare a device-space snapshot of the backdrop, for arbitrary images/gradients. The bitmap is
   * shared, non-owning from the paint's perspective, and sampled per pixel by the text painter.
   */
  void setTextBackgroundBitmap(std::shared_ptr<const Bitmap> bitmap);
  void clearTextBackgroundBitmap();
  [[nodiscard]] bool hasTextBackgroundColor() const noexcept {
    return mTextBackground.has_value() || mTextBackgroundBitmap != nullptr;
  }
  [[nodiscard]] Color textBackgroundColor() const noexcept {
    return mTextBackground.value_or(Color::Black());
  }
  /** Opt into uncached exact glyph phases (static labels only; slower than the 1/16 cache). */
  void setExactSubpixelPhase(bool exact);
  [[nodiscard]] bool exactSubpixelPhase() const noexcept {
    return mExactSubpixelPhase;
  }
  /**
   * Render the line into an offscreen coverage layer and composite it linearily over the actual
   * destination (raster backend). No caller-provided backdrop snapshot is needed.
   */
  void setLinearLightLayer(bool enabled);
  [[nodiscard]] bool linearLightLayer() const noexcept {
    return mLinearLightLayer;
  }

  /** Wrapping width; 0 means one line (the old behaviour). */
  void setWrapWidth(float width);
  [[nodiscard]] float wrapWidth() const noexcept {
    return mWrapWidth;
  }
  /** 0 means unlimited lines; a positive value clamps the laid-out lines. */
  void setMaxLines(int lines);
  [[nodiscard]] int maxLines() const noexcept {
    return mMaxLines;
  }
  /** Append an ellipsis when maxLines truncates the text. */
  void setEllipsis(bool ellipsis);
  [[nodiscard]] bool ellipsis() const noexcept {
    return mEllipsis;
  }

  /** Horizontal placement inside the wrap width, including TextAlign::Justify. */
  void setTextAlign(TextAlign align);
  [[nodiscard]] TextAlign textAlign() const noexcept {
    return mAlign;
  }
  /** Fixed line box height; 0 uses the font metrics. */
  void setLineHeight(float height);
  [[nodiscard]] float configuredLineHeight() const noexcept {
    return mConfiguredLineHeight;
  }
  /** Horizontal flow (default) or upright vertical columns. */
  void setOrientation(TextOrientation orientation);
  [[nodiscard]] TextOrientation orientation() const noexcept {
    return mOrientation;
  }
  /** Paragraph direction: Auto/Ltr/Rtl (CSS `direction`). */
  void setDirection(TextDirectionMode direction);
  [[nodiscard]] TextDirectionMode direction() const noexcept {
    return mDirection;
  }

  /** Enable mouse selection; a plain Label stays non-interactive by default. */
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
  /** Byte offset nearest @p point in the laid-out text. */
  [[nodiscard]] size_t indexAt(const Point& point) const;
  void setSelectionColor(Color color);

  Size measure(const Size& available) override;
  [[nodiscard]] Size intrinsicSize() const override;
  [[nodiscard]] float baselineOffset() const override;
  void drawSelf(RenderContext& context) override;
  void drawSelfContent(RenderContext& context);
  /** Last laid-out result (valid after measure/layout). */
  [[nodiscard]] const TextLayout& layout() const noexcept {
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
  [[nodiscard]] bool cachesOwnPaint() const override { return true; }

  [[nodiscard]] float lineHeight() const;
  void ensureLayout();
  /** Lays the text out at @p wrap and records the width used, so a later pass can tell it is stale. */
  void buildLayoutAt(float wrap);
  /** A parent assigned a new width: re-wrap at it instead of keeping the measurement width. */
  void onBoundsSizeChanged() override;
  std::string mText;
  std::shared_ptr<const Font> mFont;
  std::shared_ptr<const FontFallback> mFallback;
  bool mSystemFallback = false;
  LineBreakPolicy mPolicy;
  bool mPolicyExplicit = false;
  uint32_t mTabSize = 8;
  Color mColor{};
  std::optional<Color> mTextBackground;
  std::shared_ptr<const Bitmap> mTextBackgroundBitmap;
  bool mExactSubpixelPhase = false;
  bool mLinearLightLayer = false;
  TextLayout mLayout;
  std::vector<std::unique_ptr<TextPainter>> mPainters;
  std::vector<std::vector<std::unique_ptr<TextPainter>>> mRunPainters;
  std::unique_ptr<TextPainter> mHyphenPainter;
  [[nodiscard]] float measureText(std::string_view text, bool rightToLeft) const;
  [[nodiscard]] float xForLineOffset(const TextLine& line, size_t offset) const;
  [[nodiscard]] size_t indexAtLine(const TextLine& line, float localX) const;
  float mWrapWidth = 0.0f;
  /** The wrap width the current `mLayout` was built for; -1 means "nothing laid out yet". */
  float mLayoutWrapWidth = -1.0f;
  int mMaxLines = 0;
  bool mEllipsis = false;
  TextAlign mAlign = TextAlign::Left;
  float mConfiguredLineHeight = 0.0f;
  TextOrientation mOrientation = TextOrientation::Horizontal;
  TextDirectionMode mDirection = TextDirectionMode::Auto;
  bool mSelectable = false;
  bool mSelecting = false;
  size_t mSelectionStart = 0;
  size_t mSelectionEnd = 0;
  size_t mSelectionAnchor = 0;
  Color mSelectionColor = Color::MakeRGBA8(86, 156, 255, 90);
};

} // namespace coral::ui

#endif // CORAL_UI_TEXTLAYER_H
