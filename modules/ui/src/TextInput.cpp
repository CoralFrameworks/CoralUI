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

#include <ui/Accessibility.h>
#include <ui/TextInput.h>
#include <ui/Undo.h>
#include <ui/SystemFonts.h>
#include <ui/TextBidi.h>

#include "TextPainter.h"
#include "WidgetDrawing.h"
#include "unicode/GraphemeBreak.h"
#include "unicode/WordBreak.h"

#include <ui/Clipboard.h>
#include <ui/Menu.h>
#include <graphics/FontFallback.h>
#include <ui/Theme.h>

#include <graphics/Paint.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>

namespace coral::ui {

namespace {

/** Cleans pasted text: CR is dropped, and a single-line field flattens breaks it cannot display. */
std::string pasteText(std::string text, const bool multiline) {
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  if (!multiline) {
    std::replace(text.begin(), text.end(), '\n', ' ');
  }
  return text;
}

size_t previousCodepoint(const std::string& text, size_t index) {
  if (index == 0) return 0;
  --index;
  while (index > 0 && (static_cast<unsigned char>(text[index]) & 0xC0u) == 0x80u) {
    --index;
  }
  return index;
}

size_t nextCodepoint(const std::string& text, size_t index) {
  if (index >= text.size()) return text.size();
  ++index;
  while (index < text.size() && (static_cast<unsigned char>(text[index]) & 0xC0u) == 0x80u) {
    ++index;
  }
  return index;
}

/** Moves an arbitrary byte offset back to the start of its UTF-8 codepoint. */
size_t codepointBoundary(const std::string& text, size_t index) {
  index = std::min(index, text.size());
  while (index > 0 && index < text.size() &&
         (static_cast<unsigned char>(text[index]) & 0xC0u) == 0x80u) {
    --index;
  }
  return index;
}

char32_t codePointAt(const std::string& text, const size_t index) {
  if (index >= text.size()) return 0;
  const auto lead = static_cast<unsigned char>(text[index]);
  if (lead < 0x80u) return lead;
  const size_t length = lead < 0xE0u ? 2u : lead < 0xF0u ? 3u : 4u;
  if (index + length > text.size()) return lead;
  char32_t value = lead & (length == 2 ? 0x1Fu : length == 3 ? 0x0Fu : 0x07u);
  for (size_t offset = 1; offset < length; ++offset) {
    value = (value << 6) | (static_cast<unsigned char>(text[index + offset]) & 0x3Fu);
  }
  return value;
}

void appendUtf8(std::string* out, const char32_t character) {
  if (character <= 0x7F) {
    out->push_back(static_cast<char>(character));
  } else if (character <= 0x7FF) {
    out->push_back(static_cast<char>(0xC0 | (character >> 6)));
    out->push_back(static_cast<char>(0x80 | (character & 0x3F)));
  } else if (character <= 0xFFFF) {
    out->push_back(static_cast<char>(0xE0 | (character >> 12)));
    out->push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (character & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xF0 | (character >> 18)));
    out->push_back(static_cast<char>(0x80 | ((character >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((character >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (character & 0x3F)));
  }
}

} // namespace

TextInput::TextInput(std::string text, std::shared_ptr<const graphics::Font> font)
    : mText(std::move(text)), mFont(std::move(font)), mCursor(mText.size()),
      mSelectionAnchor(mCursor) {
  // The editor has no horizontal scrollbar: wrap overlong words. Callers can
  // replace the whole policy with setLineBreakPolicy().
  mPolicy.overflowWrap = OverflowWrap::BreakWord;
  setAccessibleRole(AccessibilityRole::TextField);
  setAccessibleEditable(true);

  // Dragging a press that lands inside an existing selection out of the field starts a platform drag
  // carrying the marked text. The press captures the text because it collapses the selection.
  setDragOutProvider([this]() -> std::optional<DragPayload> {
    if (mDragOutText.empty()) {
      return std::nullopt;
    }
    return DragPayload::text(mDragOutText);
  });
}

TextInput::~TextInput() = default;

void TextInput::setText(std::string text) {
  mText = std::move(text);
  mComposition.clear();
  mCursor = mText.size();
  mSelectionAnchor = mCursor;
  mUndo.clear();
  mRedo.clear();
  mRunPainters.clear();
  mPendingEdit.reset();
  mLayoutWidth = -1.0f;
  setAccessibleValue(mText);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ValueChanged);
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
}

void TextInput::setFont(std::shared_ptr<const graphics::Font> font) {
  if (mFont == font) return;
  mFont = std::move(font);
  markFontChanged();
  if (mSystemFallback) mFallback = systemFontFallback(mFont);
  mRunPainters.clear();
  mLayoutWidth = -1.0f;
  setAccessibleValue(mText);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ValueChanged);
  setNeedsLayout();
  setNeedsPaint();
}

void TextInput::setSystemFontFallback(const bool enabled) {
  if (mSystemFallback == enabled) return;
  mSystemFallback = enabled;
  setFontFallback(enabled ? systemFontFallback(mFont) : nullptr);
}

void TextInput::setLineBreakPolicy(const LineBreakPolicy& policy) {
  mPolicyExplicit = true;
  if (mPolicy == policy) return;
  mPolicy = policy;
  mLayoutWidth = -1.0f;
  mPendingEdit.reset();
  setNeedsLayout();
  setNeedsPaint();
}

void TextInput::setFontFallback(std::shared_ptr<const graphics::FontFallback> fallback) {
  mFallback = std::move(fallback);
  mLayoutWidth = -1.0f;
  mRunPainters.clear();
  setNeedsLayout();
  setNeedsPaint();
}

void TextInput::setMultiline(const bool multiline) {
  if (mMultiline == multiline) return;
  mMultiline = multiline;
  mLayoutWidth = -1.0f;
  setAccessibleValue(mText);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ValueChanged);
  setNeedsLayout();
  setNeedsPaint();
}

void TextInput::setColors(const graphics::Color background, const graphics::Color foreground) {
  mBackground = background;
  mForeground = foreground;
  setNeedsPaint();
}

void TextInput::setPlaceholder(std::string placeholder) {
  if (accessibleName().empty()) setAccessibleName(placeholder);
  mPlaceholder = std::move(placeholder);
  setNeedsPaint();
}

observer::Observation TextInput::observeChanged(
    std::function<void(const std::string&)> callback) const {
  return mChanged.observe(std::move(callback));
}

void TextInput::setPreferredWidth(const float width) {
  mPreferredWidth = std::max(0.0f, width);
  mLayoutWidth = -1.0f;
  setAccessibleValue(mText);
  notifyAccessibilityEvent(*this, AccessibilityEvent::ValueChanged);
  setNeedsLayout();
  setNeedsPaint();
}

std::string TextInput::selectedText() const {
  if (!hasSelection()) {
    return {};
  }
  return mText.substr(selectionStart(), selectionEnd() - selectionStart());
}

void TextInput::setCursor(const size_t cursor) {
  mCaretBlinkStart = std::chrono::steady_clock::now();
  mCaretVisibleCached = true;
  mCursor = codepointBoundary(mText, cursor);
  mSelectionAnchor = mCursor;
  mComposition.clear();
  ensureCursorVisible();
  setNeedsPaint();
}

void TextInput::setSelection(const size_t anchor, const size_t cursor) {
  mSelectionAnchor = codepointBoundary(mText, anchor);
  mCursor = codepointBoundary(mText, cursor);
  mComposition.clear();
  setNeedsPaint();
}

void TextInput::selectAll() {
  mSelectionAnchor = 0;
  mCursor = mText.size();
  mComposition.clear();
  setNeedsPaint();
}

bool TextInput::undo() {
  if (mSharedUndo != nullptr) return mSharedUndo->undo();
  if (mUndo.empty()) {
    return false;
  }
  mRedo.push_back(mText);
  mText = std::move(mUndo.back());
  mUndo.pop_back();
  mCursor = mText.size();
  mSelectionAnchor = mCursor;
  mComposition.clear();
  mLayoutWidth = -1.0f;
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
  return true;
}

bool TextInput::redo() {
  if (mSharedUndo != nullptr) return mSharedUndo->redo();
  if (mRedo.empty()) {
    return false;
  }
  mUndo.push_back(mText);
  mText = std::move(mRedo.back());
  mRedo.pop_back();
  mCursor = mText.size();
  mSelectionAnchor = mCursor;
  mComposition.clear();
  mLayoutWidth = -1.0f;
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
  return true;
}

Size TextInput::measure(const Size& available) {
  if (mFont == nullptr) {
    setMeasuredSize({ mPreferredWidth, 32.0f });
    return measuredSize();
  }
  const float height = mFont->metrics().ascent + mFont->metrics().descent + 18.0f;
  if (!mMultiline) {
    setMeasuredSize({ mPreferredWidth, height });
    return measuredSize();
  }
  float width = mPreferredWidth;
  if (width <= 0.0f) {
    width = available.width > 0.0f ? available.width : bounds().getWidth();
  }
  const float wrapWidth = std::max(1.0f, width - mPadding * 2.0f);
  ensureLayout(wrapWidth);
  const float lines = std::max(1.0f, static_cast<float>(mLayout.lines.size()));
  setMeasuredSize({ width, std::max(height, lines * lineHeight() + 18.0f) });
  return measuredSize();
}

float TextInput::lineHeight() const {
  if (mLayout.lineHeight > 0.0f) return mLayout.lineHeight;
  if (mFont == nullptr) return 16.0f;
  const auto& metrics = mFont->metrics();
  return metrics.ascent + metrics.descent + metrics.lineGap;
}

void TextInput::ensureLayout(const float width) const {
  if (mFont == nullptr) {
    mLayout = TextLayout{};
    mLayoutWidth = width;
    return;
  }
  TextLayoutOptions options{ mMultiline ? width : 0.0f, 0, false };
  options.policy = mPolicy;
  const bool sameWidth = mLayoutWidth == width;
  if (sameWidth && mLayout.source != nullptr && mLayout.source->equals(mText) &&
      !mLayout.lines.empty()) {
    return;
  }
  if (sameWidth && mFallback == nullptr && mLayout.source != nullptr) {
    if (mPendingEdit.has_value()) {
      mLayout = updateTextLayout(std::move(mLayout), mText, *mPendingEdit, *mFont, options);
    } else {
      mLayout = updateTextLayout(std::move(mLayout), mText, *mFont, options);
    }
    mPendingEdit.reset();
    return;
  }
  mLayoutWidth = width;
  mPendingEdit.reset();
  mLayout = mFallback != nullptr
    ? layoutText(mText, *mFont, *mFallback, options)
    : layoutText(mText, *mFont, options);
  return;
}

size_t TextInput::lineStart(const int line) const {
  if (mLayout.lines.empty()) return 0;
  const int index = std::clamp(line, 0, static_cast<int>(mLayout.lines.size()) - 1);
  return mLayout.lines[static_cast<size_t>(index)].start;
}

size_t TextInput::lineEnd(const int line) const {
  if (mLayout.lines.empty()) return mText.size();
  const int index = std::clamp(line, 0, static_cast<int>(mLayout.lines.size()) - 1);
  return mLayout.lines[static_cast<size_t>(index)].end;
}

size_t TextInput::visualCaretNeighbor(const bool left) const {
  ensureLayout(mMultiline ? std::max(1.0f, bounds().getWidth() - mPadding * 2.0f) : 0.0f);
  if (mLayout.lines.empty()) {
    return left ? unicode::previousGraphemeBoundary(mText, mCursor)
                : unicode::nextGraphemeBoundary(mText, mCursor);
  }
  const size_t lineIndex = std::min(lineIndexForCursor(mCursor), mLayout.lines.size() - 1);
  const TextLine& line = mLayout.lines[lineIndex];
  if (line.visualRuns.empty()) {
    return left ? unicode::previousGraphemeBoundary(mText, mCursor)
                : unicode::nextGraphemeBoundary(mText, mCursor);
  }

  // Build the caret stops in visual (left-to-right) order for this line. RTL
  // runs contribute their grapheme boundaries in reverse.
  std::vector<size_t> stops;
  stops.reserve(line.text.size() + 1);
  const auto pushStop = [&](const size_t offset) {
    if (stops.empty() || stops.back() != offset) stops.push_back(offset);
  };
  for (const uint32_t runIndex: line.visualRuns.order()) {
    const TextVisualRun run = line.visualRuns[runIndex];
    if (run.start < line.start || run.end > line.end || run.end <= run.start) continue;
    const std::string_view runText = line.text.substr(run.start - line.start, run.end - run.start);
    const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(runText);
    if (run.level % 2 == 0) {
      for (const size_t boundary: boundaries) pushStop(run.start + boundary);
    } else {
      for (auto iterator = boundaries.rbegin(); iterator != boundaries.rend(); ++iterator) {
        pushStop(run.start + *iterator);
      }
    }
  }
  const auto found = std::find(stops.begin(), stops.end(), mCursor);
  if (found == stops.end()) {
    return left ? unicode::previousGraphemeBoundary(mText, mCursor)
                : unicode::nextGraphemeBoundary(mText, mCursor);
  }
  if (left) {
    if (found != stops.begin()) return *(found - 1);
    return lineIndex > 0 ? mLayout.lines[lineIndex - 1].end : mCursor;
  }
  const auto next = found + 1;
  if (next != stops.end()) return *next;
  return lineIndex + 1 < mLayout.lines.size() ? mLayout.lines[lineIndex + 1].start : mCursor;
}

size_t TextInput::lineIndexForCursor(const size_t cursor) const {
  if (mLayout.lines.empty()) return 0;
  for (size_t index = 0; index < mLayout.lines.size(); ++index) {
    if (cursor <= mLayout.lines[index].end) return index;
  }
  return mLayout.lines.size() - 1;
}

float TextInput::xForIndexInLine(const size_t index, const TextLine& line) const {
  if (mFont == nullptr) return 0.0f;
  const size_t begin = std::min(line.start, mText.size());
  const size_t end = std::min(std::max(index, begin), mText.size());
  if (line.visualRuns.empty()) {
    const std::string_view value = std::string_view(mText).substr(begin, end - begin);
    return mFallback != nullptr ? mFallback->measure(value) : mFont->measureWidth(value);
  }

  const std::vector<uint32_t>& order = line.visualRuns.order();
  const size_t clamped = std::min(std::max(index, line.start), std::min(line.end, mText.size()));
  float left = 0.0f;
  for (const size_t runIndex: order) {
    const TextVisualRun& run = line.visualRuns[runIndex];
    if (run.start < line.start || run.end > mText.size()) continue;
    const std::string runText(mText.substr(run.start, run.end - run.start));
    const bool rightToLeft = run.level % 2 != 0;
    const std::string painted = rightToLeft ? mirrorBidiText(runText) : runText;
    const graphics::Font::Direction direction = rightToLeft
      ? graphics::Font::Direction::RightToLeft
      : graphics::Font::Direction::LeftToRight;
    const float width = mFallback != nullptr
      ? mFallback->measure(painted)
      : detail::TextShapeCache::instance().measure(*mFont, painted, direction);
    if (clamped < run.start || clamped > run.end) {
      left += width;
      continue;
    }
    const std::string prefix = runText.substr(0, clamped - run.start);
    const std::string paintedPrefix = rightToLeft ? mirrorBidiText(prefix) : prefix;
    const float prefixWidth = mFallback != nullptr
      ? mFallback->measure(paintedPrefix)
      : detail::TextShapeCache::instance().measure(*mFont, paintedPrefix, direction);
    return rightToLeft ? left + width - prefixWidth : left + prefixWidth;
  }
  return left;
}

size_t TextInput::indexAtPoint(const Point& point) const {
  if (mFont == nullptr) return 0;
  const float line = std::max(0.0f, (point.y - bounds().getTop() - 9.0f + mScrollY) / lineHeight());
  const int lineIndex = mLayout.lines.empty() ? 0
    : std::clamp(static_cast<int>(line), 0, static_cast<int>(mLayout.lines.size()) - 1);
  if (mLayout.lines.empty()) return 0;
  const TextLine& entry = mLayout.lines[static_cast<size_t>(lineIndex)];
  const float local = point.x - bounds().getLeft() - mPadding;
  const std::string lineText = mText.substr(entry.start, entry.end - entry.start);
  const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(lineText);
  size_t best = entry.start;
  float bestDistance = std::abs(local);
  const auto measureGrapheme = [&](const std::string_view value) {
    return mFallback != nullptr ? mFallback->measure(value) : mFont->measureWidth(value);
  };
  if (entry.visualRuns.empty()) {
    float advance = 0.0f;
    size_t previous = 0;
    for (const size_t boundary: boundaries) {
      if (boundary == 0) continue;
      advance += measureGrapheme(std::string_view(lineText).substr(previous, boundary - previous));
      const float distance = std::abs(local - advance);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = entry.start + boundary;
      }
      previous = boundary;
    }
    return best;
  }

  const std::vector<uint32_t>& order = entry.visualRuns.order();
  float runLeft = 0.0f;
  for (const size_t runIndex: order) {
    const TextVisualRun run = entry.visualRuns[runIndex];
    if (run.start < entry.start || run.end > entry.end || run.end <= run.start) continue;
    const std::string runText = mText.substr(run.start, run.end - run.start);
    const bool rightToLeft = run.level % 2 != 0;
    const float width = rightToLeft
      ? measureGrapheme(mirrorBidiText(runText))
      : measureGrapheme(runText);
    if (local > runLeft + width) {
      runLeft += width;
      continue;
    }
    const std::vector<size_t> runBoundaries = unicode::computeGraphemeBreaks(runText);
    float advance = 0.0f;
    size_t previous = 0;
    for (const size_t boundary: runBoundaries) {
      if (boundary == 0) continue;
      const std::string_view grapheme = std::string_view(runText).substr(previous, boundary - previous);
      advance += rightToLeft
        ? measureGrapheme(mirrorBidiText(grapheme))
        : measureGrapheme(grapheme);
      const float candidate = rightToLeft ? runLeft + width - advance : runLeft + advance;
      const float distance = std::abs(local - candidate);
      if (distance < bestDistance) {
        bestDistance = distance;
        best = run.start + boundary;
      }
      previous = boundary;
    }
    return best;
  }
  return entry.end;
}

void TextInput::drawLine(RenderContext& context, const TextLine& line, const float left,
    const float baseline, const graphics::Paint& paint, const size_t painterIndex) {
  const std::string hyphenText = "\xE2\x80\x90";
  const float hyphenWidth = mFallback != nullptr
    ? mFallback->measure(hyphenText)
    : detail::TextShapeCache::instance().measure(*mFont, hyphenText);
  const auto drawHyphen = [&]() {
    if (!line.hyphenated) return;
    if (mHyphenPainter == nullptr) mHyphenPainter = std::make_unique<TextPainter>();
    mHyphenPainter->draw(context, hyphenText, left + line.width - hyphenWidth, baseline, *mFont, paint);
  };
  if (line.text.empty()) return;
  if (line.visualRuns.empty()) {
    if (mFallback != nullptr) {
      const std::vector<graphics::FontRun> runs = mFallback->shape(line.text);
      if (mRunPainters.size() <= painterIndex) mRunPainters.resize(painterIndex + 1);
      if (mRunPainters[painterIndex].size() < runs.size()) {
        mRunPainters[painterIndex].resize(runs.size());
      }
      float x = left;
      for (size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
        const graphics::FontRun& run = runs[runIndex];
        if (run.font == nullptr) continue;
        if (mRunPainters[painterIndex][runIndex] == nullptr) {
          mRunPainters[painterIndex][runIndex] = std::make_unique<TextPainter>();
        }
        mRunPainters[painterIndex][runIndex]->draw(context, run.text, x, baseline, *run.font, paint);
        x += run.width;
      }
    } else {
      if (mTextPainter == nullptr) mTextPainter = std::make_unique<TextPainter>();
      mTextPainter->draw(context, line.text, left, baseline, *mFont, paint,
        graphics::Font::Direction::Auto, line.shaped);
    }
    drawHyphen();
    return;
  }

  const std::vector<uint32_t>& order = line.visualRuns.order();
  float x = left;
  for (const size_t runIndex: order) {
    const TextVisualRun& run = line.visualRuns[runIndex];
    if (run.start < line.start || run.end > mText.size()) continue;
    const std::string runText(mText.substr(run.start, run.end - run.start));
    const bool rightToLeft = run.level % 2 != 0;
    const std::string painted = rightToLeft ? mirrorBidiText(runText) : runText;
    const graphics::Font::Direction direction = rightToLeft
      ? graphics::Font::Direction::RightToLeft
      : graphics::Font::Direction::LeftToRight;
    if (mFallback != nullptr) {
      const std::vector<graphics::FontRun> runs = mFallback->shape(painted);
      TextPainter painter;
      for (const graphics::FontRun& fontRun: runs) {
        if (fontRun.font == nullptr) continue;
        painter.draw(context, fontRun.text, x, baseline, *fontRun.font, paint, direction);
        x += fontRun.width;
      }
    } else {
      if (mTextPainter == nullptr) mTextPainter = std::make_unique<TextPainter>();
      std::shared_ptr<const graphics::TextBlob> runBlob;
      const auto lazyRun = line.visualRuns.lazy(runIndex);
        if (runBlob == nullptr && lazyRun != nullptr) runBlob = lazyRun->get();
        mTextPainter->draw(context, painted, x, baseline, *mFont, paint, direction, runBlob);
      x += detail::TextShapeCache::instance().measure(*mFont, painted, direction);
    }
  }
  drawHyphen();
}

void TextInput::drawSelectionForLine(RenderContext& context, const TextLine& line,
    const float left, const float top, const float height, const size_t begin, const size_t end) {
  if (begin >= end) return;
  graphics::Paint selection;
  selection.color = mFocus;
  selection.color.alpha = 0.35f;
  selection.style = graphics::Paint::Style::kFill;
  if (line.visualRuns.empty()) {
    const float firstX = left + xForIndexInLine(begin, line);
    const float secondX = left + xForIndexInLine(end, line);
    context.canvas().drawRect(Rect::MakeXYWH(std::min(firstX, secondX), top,
      std::max(1.0f, std::abs(secondX - firstX)), height), selection);
    return;
  }

  for (const TextVisualRun& run: line.visualRuns) {
    const size_t runBegin = std::max(begin, run.start);
    const size_t runEnd = std::min(end, run.end);
    if (runBegin >= runEnd) continue;
    const float firstX = left + xForIndexInLine(runBegin, line);
    const float secondX = left + xForIndexInLine(runEnd, line);
    context.canvas().drawRect(Rect::MakeXYWH(std::min(firstX, secondX), top,
      std::max(1.0f, std::abs(secondX - firstX)), height), selection);
  }
}

void TextInput::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  if (mBordered) {
    graphics::Paint background;
    background.color = mBackground;
    background.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), background);
    graphics::Paint border;
    border.color = mFocused ? mFocus : mBorder;
    border.style = graphics::Paint::Style::kStroke;
    border.strokeWidth = 1.0f;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);
    detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
  }
  if (mFont == nullptr) {
    return;
  }
  graphics::Paint text;
  text.color = mForeground;
  text.style = graphics::Paint::Style::kFill;
  const float left = bounds().getLeft() + mPadding;
  const float lineHeightValue = lineHeight();
  const graphics::FontMetrics& metrics = mFont->metrics();
  if (mMultiline) {
    ensureLayout(std::max(1.0f, bounds().getWidth() - mPadding * 2.0f));
    canvas.save();
    canvas.clipRect(bounds());
    const float top = bounds().getTop() + 9.0f - mScrollY;
    const size_t selectionBegin = selectionStart();
    const size_t selectionEndValue = selectionEnd();
    const size_t count = std::max<size_t>(1, mLayout.lines.size());
    for (size_t index = 0; index < count; ++index) {
      TextLine line;
      if (index < mLayout.lines.size()) {
        line = mLayout.lines[index];
      } else {
        line.start = mText.size();
        line.end = mText.size();
      }
      const float lineTop = top + static_cast<float>(index) * lineHeightValue;
      const float baseline = lineTop + metrics.ascent;
      if (hasSelection()) {
        drawSelectionForLine(context, line, left, lineTop + 3.0f, lineHeightValue - 6.0f,
          std::max(selectionBegin, line.start), std::min(selectionEndValue, line.end));
      }
      drawLine(context, line, left, baseline, text, index);
    }
    if (mText.empty() && !mPlaceholder.empty()) {
      if (mPlaceholderPainter == nullptr) mPlaceholderPainter = std::make_unique<TextPainter>();
      graphics::Paint placeholder = text;
      placeholder.color = mMuted;
      mPlaceholderPainter->draw(context, mPlaceholder, left,
        top + metrics.ascent, *mFont, placeholder);
    }
    if (mFocused) {
      const size_t lineIndex = lineIndexForCursor(mCursor);
      const TextLine& line = mLayout.lines.empty() ? TextLine{}
        : mLayout.lines[std::min(lineIndex, mLayout.lines.size() - 1)];
      const float caretX = left + xForIndexInLine(mCursor, line);
      const float caretTop = top + static_cast<float>(lineIndex) * lineHeightValue + 3.0f;
      graphics::Paint caret;
      caret.color = mForeground;
      caret.style = graphics::Paint::Style::kFill;
      if (caretVisibleAt(std::chrono::steady_clock::now())) {
        canvas.drawRect(Rect::MakeXYWH(caretX, caretTop, 1.5f, lineHeightValue - 6.0f), caret);
      }
      if (!mComposition.empty()) {
        if (mCompositionPainter == nullptr) mCompositionPainter = std::make_unique<TextPainter>();
        graphics::Paint composed = text;
        composed.color = mMuted;
        const float baseline = top + static_cast<float>(lineIndex) * lineHeightValue + metrics.ascent;
        mCompositionPainter->draw(context, mComposition, caretX, baseline, *mFont, composed);
        const float width = mFont->measureWidth(mComposition);
        canvas.drawRect(Rect::MakeXYWH(caretX, baseline + 2.0f, width, 1.0f), caret);
      }
    }
    canvas.restore();
    return;
  }

  ensureLayout(0.0f);
  const float baseline = bounds().getTop() + (bounds().getHeight() +
    metrics.ascent - metrics.descent) * 0.5f;
  if (hasSelection()) {
    if (!mLayout.lines.empty()) {
      drawSelectionForLine(context, mLayout.lines.front(), left, bounds().getTop() + 6.0f,
        bounds().getHeight() - 12.0f, selectionStart(), selectionEnd());
    } else {
      const float firstX = left + xForIndex(selectionStart());
      const float secondX = left + xForIndex(selectionEnd());
      const float selectionLeft = std::min(firstX, secondX);
      const float selectionRight = std::max(firstX, secondX);
      graphics::Paint selection;
      selection.color = mFocus;
      selection.color.alpha = 0.35f;
      selection.style = graphics::Paint::Style::kFill;
      canvas.drawRect(Rect::MakeXYWH(selectionLeft, bounds().getTop() + 6.0f,
        std::max(1.0f, selectionRight - selectionLeft), bounds().getHeight() - 12.0f), selection);
    }
  }
  if (mText.empty() && !mPlaceholder.empty()) {
    if (mPlaceholderPainter == nullptr) mPlaceholderPainter = std::make_unique<TextPainter>();
    graphics::Paint placeholder = text;
    placeholder.color = mMuted;
    mPlaceholderPainter->draw(context, mPlaceholder, left, baseline, *mFont, placeholder);
  } else if (!mLayout.lines.empty()) {
    drawLine(context, mLayout.lines.front(), left, baseline, text, 0);
  } else if (mFallback != nullptr) {
    const std::vector<graphics::FontRun> runs = mFallback->shape(mText);
    if (mRunPainters.empty()) mRunPainters.resize(1);
    if (mRunPainters[0].size() < runs.size()) mRunPainters[0].resize(runs.size());
    float x = left;
    for (size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
      const graphics::FontRun& run = runs[runIndex];
      if (run.font == nullptr) continue;
      if (mRunPainters[0][runIndex] == nullptr) {
        mRunPainters[0][runIndex] = std::make_unique<TextPainter>();
      }
      mRunPainters[0][runIndex]->draw(context, run.text, x, baseline, *run.font, text);
      x += run.width;
    }
  } else {
    if (mTextPainter == nullptr) mTextPainter = std::make_unique<TextPainter>();
    mTextPainter->draw(context, mText, left, baseline, *mFont, text);
  }
  if (mFocused) {
    const float caretX = left + xForIndex(mCursor);
    graphics::Paint caret;
    caret.color = mForeground;
    caret.style = graphics::Paint::Style::kFill;
    if (caretVisibleAt(std::chrono::steady_clock::now())) {
      canvas.drawRect(Rect::MakeXYWH(caretX, bounds().getTop() + 7.0f, 1.5f,
        bounds().getHeight() - 14.0f), caret);
    }
    if (!mComposition.empty()) {
      if (mCompositionPainter == nullptr) mCompositionPainter = std::make_unique<TextPainter>();
      graphics::Paint composed = text;
      composed.color = mMuted;
      mCompositionPainter->draw(context, mComposition, caretX, baseline, *mFont, composed);
      const float width = mFont->measureWidth(mComposition);
      canvas.drawRect(Rect::MakeXYWH(caretX, baseline + 2.0f, width, 1.0f), caret);
    }
  }
}

Rect TextInput::imeCursorRect() const {
  if (!mFocused || mFont == nullptr) {
    return {};
  }
  const float left = bounds().getLeft() + mPadding;
  // The candidate window is anchored to this rectangle. While a composition is in progress the
  // preedit is drawn at the caret, so the anchor must span it or the candidates cover the text.
  const float compositionWidth = mComposition.empty()
                                   ? 0.0f
                                   : (mFallback != nullptr ? mFallback->measure(mComposition)
                                                           : mFont->measureWidth(mComposition));
  if (mMultiline) {
    ensureLayout(std::max(1.0f, bounds().getWidth() - mPadding * 2.0f));
    const size_t lineIndex = lineIndexForCursor(mCursor);
    const TextLine& line = mLayout.lines.empty()
      ? TextLine{}
      : mLayout.lines[std::min(lineIndex, mLayout.lines.size() - 1)];
    const float baseline = bounds().getTop() + 9.0f - mScrollY +
      static_cast<float>(lineIndex) * lineHeight() + mFont->metrics().ascent;
    return Rect::MakeXYWH(left + xForIndexInLine(mCursor, line),
      baseline - mFont->metrics().ascent, 2.0f + compositionWidth, lineHeight());
  }
  const float baseline = bounds().getTop() + (bounds().getHeight() +
    mFont->metrics().ascent - mFont->metrics().descent) * 0.5f;
  return Rect::MakeXYWH(left + xForIndex(mCursor), baseline - mFont->metrics().ascent,
    2.0f + compositionWidth, mFont->metrics().ascent + mFont->metrics().descent);
}

bool TextInput::onPointerDown(const PointerEvent& event) {
  if (event.button == PointerButton::Middle) {
    // X11/Wayland middle-click paste reads the PRIMARY selection, not the clipboard.
    pastePrimarySelection();
    return true;
  }
  if (event.button == PointerButton::Secondary && mFont != nullptr) {
    if (!hasSelection()) {
      const size_t cursor = mMultiline ? indexAtPoint(event.position) : indexAtX(event.position.x);
      mCursor = cursor;
      mSelectionAnchor = cursor;
    }
    showContextMenu(event.position);
    setNeedsPaint();
    return true;
  }
  if (mContextMenu != nullptr) mContextMenu->hide();
  const size_t cursor = mMultiline ? indexAtPoint(event.position) : indexAtX(event.position.x);
  // The press collapses the selection, so capture the marked text first: a drag from inside the
  // selection is a drag-out, not a new selection gesture.
  mDragOutText = hasSelection() && cursor >= selectionStart() && cursor <= selectionEnd()
                   ? selectedText()
                   : std::string{};
  const auto now = std::chrono::steady_clock::now();
  const double elapsed = std::chrono::duration<double, std::milli>(now - mLastClick).count();
  const float dx = event.position.x - mLastClickPosition.x;
  const float dy = event.position.y - mLastClickPosition.y;
  const bool near = dx * dx + dy * dy < 36.0f;
  mClickCount = (elapsed < 400.0 && near) ? std::min(3, mClickCount + 1) : 1;
  mLastClick = now;
  mLastClickPosition = event.position;
  if (!event.shift && mClickCount >= 2) {
    const auto range = mClickCount >= 3 ? lineRangeAt(cursor) : wordRangeAt(cursor);
    mSelectionAnchor = range.first;
    mCursor = range.second;
  } else if (event.shift) {
    mCursor = cursor;
  } else {
    mCursor = cursor;
    mSelectionAnchor = cursor;
  }
  mSelecting = true;
  ensureCursorVisible();
  mComposition.clear();
  setNeedsPaint();
  return true;
}

bool TextInput::onPointerMove(const PointerEvent& event) {
  if (!mSelecting) return false;
  if (mMultiline) {
    constexpr float kEdge = 12.0f;
    if (event.position.y < bounds().getTop() + kEdge) scrollBy(-lineHeight());
    else if (event.position.y > bounds().getBottom() - kEdge) scrollBy(lineHeight());
  }
  mCursor = mMultiline ? indexAtPoint(event.position) : indexAtX(event.position.x);
  mComposition.clear();
  setNeedsPaint();
  return true;
}

bool TextInput::onPointerUp(const PointerEvent& event) {
  (void)event;
  mSelecting = false;
  // A drag just finished: publish what is marked so another application can middle-click paste it.
  publishPrimarySelection();
  // The drag-out payload belongs to the press that armed it; a later press recaptures it.
  mDragOutText.clear();
  return true;
}

bool TextInput::onKeyDown(const KeyEvent& event) {
  const bool command = event.control || event.meta;
  if (command) {
    switch (event.key) {
      case Key::A:
        selectAll();
        return true;
      case Key::C:
        copySelection();
        return true;
      case Key::X:
        cutSelection();
        return true;
      case Key::V:
        paste();
        return true;
      case Key::Z:
        return event.shift ? redo() : undo();
      case Key::Y:
        return redo();
      case Key::Backspace:
        eraseWordBefore();
        return true;
      case Key::Delete:
        eraseWordAfter();
        return true;
      default:
        break;
    }
  }

  switch (event.key) {
    case Key::Backspace:
      eraseBefore();
      return true;
    case Key::Delete:
      eraseAfter();
      return true;
    case Key::Left:
      if (!event.shift && hasSelection()) {
        setCursor(selectionStart());
      } else {
        moveCursor(visualCaretNeighbor(true), event.shift);
      }
      return true;
    case Key::Right:
      if (!event.shift && hasSelection()) {
        setCursor(selectionEnd());
      } else {
        moveCursor(visualCaretNeighbor(false), event.shift);
      }
      return true;
    case Key::Up:
    case Key::Down: {
      if (!mMultiline) return false;
      ensureLayout(std::max(1.0f, bounds().getWidth() - mPadding * 2.0f));
      if (mLayout.lines.empty()) return false;
      const int line = static_cast<int>(lineIndexForCursor(mCursor));
      const int target = std::clamp(line + (event.key == Key::Down ? 1 : -1), 0,
        std::max(0, static_cast<int>(mLayout.lines.size()) - 1));
      const TextLine& current = mLayout.lines[std::min(static_cast<size_t>(line),
        mLayout.lines.size() - 1)];
      const float x = xForIndexInLine(mCursor, current);
      const Point point{ bounds().getLeft() + mPadding + x,
        bounds().getTop() + 9.0f + static_cast<float>(target) * lineHeight() + 1.0f };
      moveCursor(indexAtPoint(point), event.shift);
      return true;
    }
    case Key::Home: {
      const size_t target = mMultiline
        ? lineStart(static_cast<int>(lineIndexForCursor(mCursor))) : 0;
      if (!event.shift && hasSelection()) setCursor(selectionStart());
      else moveCursor(target, event.shift);
      return true;
    }
    case Key::End: {
      const size_t target = mMultiline
        ? lineEnd(static_cast<int>(lineIndexForCursor(mCursor))) : mText.size();
      if (!event.shift && hasSelection()) setCursor(selectionEnd());
      else moveCursor(target, event.shift);
      return true;
    }
    case Key::Escape:
      setCursor(mCursor);
      return true;
    case Key::Enter:
      if (mMultiline) replaceSelection("\n");
      return true;
    default:
      return false;
  }
}

bool TextInput::onTextInput(const char32_t character) {
  if (character < 0x20 || character == 0x7F) {
    return false;
  }
  insert(character);
  return true;
}

bool TextInput::onTextComposition(const std::string& text) {
  mComposition = text;
  setNeedsPaint();
  return true;
}

bool TextInput::onTextCommit(const std::string& text) {
  mComposition.clear();
  if (!text.empty() || hasSelection()) {
    replaceSelection(text);
  }
  return true;
}

bool TextInput::onTextDelete(const int32_t index, const uint32_t length) {
  if (length == 0) return true;
  const auto start = static_cast<int64_t>(mCursor) + index;
  const auto size = static_cast<int64_t>(mText.size());
  const auto clampedStart = std::clamp<int64_t>(start, 0, size);
  const auto clampedLength = std::min<int64_t>(length, size - clampedStart);
  if (clampedLength <= 0) return true;
  pushUndo();
  mText.erase(static_cast<size_t>(clampedStart), static_cast<size_t>(clampedLength));
  recordEdit(static_cast<size_t>(clampedStart),
    static_cast<size_t>(clampedStart) + static_cast<size_t>(clampedLength),
    static_cast<size_t>(clampedStart));
  if (static_cast<size_t>(clampedStart) < mCursor) {
    mCursor = static_cast<size_t>(clampedStart);
  } else {
    mCursor = std::min(mCursor, mText.size());
  }
  mSelectionAnchor = mCursor;
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
  return true;
}

void TextInput::onFocusChanged(const bool focused) {
  mCaretBlinkStart = std::chrono::steady_clock::now();
  mCaretVisibleCached = true;
  mFocused = focused;
  if (!focused) {
    mComposition.clear();
    mSelecting = false;
    if (mContextMenu != nullptr) mContextMenu->hide();
  }
  setNeedsPaint();
}

void TextInput::insert(const char32_t character) {
  std::string encoded;
  appendUtf8(&encoded, character);
  replaceSelection(encoded);
}

void TextInput::eraseBefore() {
  if (hasSelection()) {
    eraseSelection();
    return;
  }
  if (mCursor == 0) return;
  pushUndo();
  const size_t previous = unicode::previousGraphemeBoundary(mText, mCursor);
  mText.erase(previous, mCursor - previous);
  recordEdit(previous, mCursor, previous);
  mCursor = previous;
  mSelectionAnchor = mCursor;
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
}

void TextInput::eraseAfter() {
  if (hasSelection()) {
    eraseSelection();
    return;
  }
  if (mCursor >= mText.size()) return;
  pushUndo();
  const size_t next = unicode::nextGraphemeBoundary(mText, mCursor);
  mText.erase(mCursor, next - mCursor);
  recordEdit(mCursor, next, mCursor);
  mSelectionAnchor = mCursor;
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
}

void TextInput::recordEdit(const size_t start, const size_t oldEnd, const size_t newEnd) {
  if (mPendingEdit.has_value()) {
    mPendingEdit.reset();
  } else {
    mPendingEdit = TextEditRange{ start, oldEnd, newEnd };
  }
}

void TextInput::replaceSelection(const std::string& text) {
  if (text.empty() && !hasSelection()) {
    return;
  }
  pushUndo();
  const size_t start = selectionStart();
  const size_t end = selectionEnd();
  mText.replace(start, end - start, text);
  recordEdit(start, end, start + text.size());
  mCursor = start + text.size();
  mSelectionAnchor = mCursor;
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
}

void TextInput::eraseSelection() {
  if (!hasSelection()) {
    return;
  }
  pushUndo();
  const size_t start = selectionStart();
  const size_t end = selectionEnd();
  mText.erase(start, end - start);
  recordEdit(start, end, start);
  mCursor = start;
  mSelectionAnchor = mCursor;
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
}

std::pair<size_t, size_t> TextInput::wordRangeAt(const size_t index) const {
  if (mText.empty()) return { 0, 0 };
  return unicode::wordRangeAt(mText, std::min(index, mText.size()));
}

std::pair<size_t, size_t> TextInput::lineRangeAt(const size_t index) const {
  if (!mMultiline) return { 0, mText.size() };
  ensureLayout(std::max(1.0f, bounds().getWidth() - mPadding * 2.0f));
  if (mLayout.lines.empty()) return { 0, mText.size() };
  const size_t line = lineIndexForCursor(index);
  return { lineStart(static_cast<int>(line)), lineEnd(static_cast<int>(line)) };
}

void TextInput::eraseWordBefore() {
  if (hasSelection()) {
    eraseSelection();
    return;
  }
  if (mCursor == 0) return;
  const auto range = wordRangeAt(unicode::previousGraphemeBoundary(mText, mCursor));
  if (range.first >= mCursor) return;
  pushUndo();
  mText.erase(range.first, mCursor - range.first);
  recordEdit(range.first, mCursor, range.first);
  mCursor = range.first;
  mSelectionAnchor = mCursor;
  setNeedsLayout();
  setNeedsPaint();
  ensureCursorVisible();
  notifyChanged();
}

void TextInput::eraseWordAfter() {
  if (hasSelection()) {
    eraseSelection();
    return;
  }
  if (mCursor >= mText.size()) return;
  const auto range = wordRangeAt(mCursor);
  if (range.second <= mCursor) return;
  pushUndo();
  mText.erase(mCursor, range.second - mCursor);
  recordEdit(mCursor, range.second, mCursor);
  mSelectionAnchor = mCursor;
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
}

void TextInput::ensureCursorVisible() {
  if (!mMultiline || mFont == nullptr || bounds().getHeight() <= 0.0f) return;
  ensureLayout(std::max(1.0f, bounds().getWidth() - mPadding * 2.0f));
  const size_t line = lineIndexForCursor(mCursor);
  const float rowTop = static_cast<float>(line) * lineHeight();
  const float rowBottom = rowTop + lineHeight();
  const float viewHeight = bounds().getHeight() - 18.0f;
  if (rowTop < mScrollY) mScrollY = rowTop;
  else if (rowBottom > mScrollY + viewHeight) mScrollY = rowBottom - viewHeight;
  const float content = std::max(0.0f, mLayout.size.height + 18.0f - bounds().getHeight());
  mScrollY = std::clamp(mScrollY, 0.0f, content);
}

void TextInput::scrollBy(const float delta) {
  if (!mMultiline || mFont == nullptr) return;
  ensureLayout(std::max(1.0f, bounds().getWidth() - mPadding * 2.0f));
  const float content = std::max(0.0f, mLayout.size.height + 18.0f - bounds().getHeight());
  mScrollY = std::clamp(mScrollY + delta, 0.0f, content);
  setNeedsPaint();
}

void TextInput::ensureContextMenu() {
  if (mContextMenu != nullptr || mFont == nullptr) return;
  // Built on first use rather than in the constructor: the labels come from the theme's translator,
  // and a control is theme-less until it is added to a tree.
  mContextMenu = std::make_shared<Menu>(mFont);
  mContextMenu->addItem(tr("ui.textinput.cut", "Cut"), [this] { cutSelection(); });
  mContextMenu->addItem(tr("ui.textinput.copy", "Copy"), [this] { copySelection(); });
  mContextMenu->addItem(tr("ui.textinput.paste", "Paste"), [this] { paste(); });
  mContextMenu->addSeparator();
  mContextMenu->addItem(tr("ui.textinput.selectAll", "Select All"), [this] { selectAll(); });
  addChild(mContextMenu);
}

void TextInput::showContextMenu(const Point& position) {
  ensureContextMenu();
  if (mContextMenu == nullptr) return;
  updateContextMenu();
  mContextMenu->showAt(position);
}

void TextInput::updateContextMenu() {
  if (mContextMenu == nullptr) return;
  mContextMenu->setItemEnabled(0, hasSelection());
  mContextMenu->setItemEnabled(1, hasSelection());
  mContextMenu->setItemEnabled(2, !Clipboard::system()->getText().empty());
}

void TextInput::copySelection() const {
  if (!hasSelection()) {
    return;
  }
  Clipboard::system()->setText(selectedText());
}

void TextInput::cutSelection() {
  if (!hasSelection()) {
    return;
  }
  copySelection();
  eraseSelection();
}

void TextInput::paste() {
  const std::string text = pasteText(Clipboard::system()->getText(), mMultiline);
  if (text.empty() && !hasSelection()) {
    return;
  }
  replaceSelection(text);
}

void TextInput::publishPrimarySelection() {
  if (!hasSelection()) {
    return;
  }
  // X11/Wayland convention: marking text publishes it to the PRIMARY selection, and another
  // application pastes it with a middle click. Platforms without one ignore the call.
  Clipboard::system()->setPrimary("text/plain", selectedText());
}

void TextInput::pastePrimarySelection() {
  const std::string text = pasteText(Clipboard::system()->getPrimary("text/plain"), mMultiline);
  if (text.empty() && !hasSelection()) {
    return;
  }
  replaceSelection(text);
}

void TextInput::setUndoStack(std::shared_ptr<UndoStack> stack) {
  mSharedUndo = std::move(stack);
}

void TextInput::pushUndo() {
  if (mSharedUndo != nullptr) {
    // The shared history stores snapshots; the "new" text is captured the first time the action is
    // undone, which is when the replaced value is known for certain.
    auto redoText = std::make_shared<std::string>();
    const std::string before = mText;
    const auto apply = [this](std::string value) {
      mText = std::move(value);
      mComposition.clear();
      mCursor = mText.size();
      mSelectionAnchor = mCursor;
      mLayoutWidth = -1.0f;
      mRunPainters.clear();
      setNeedsPaint();
    };
    mSharedUndo->push(std::make_unique<FunctionUndoAction>(
      [this, apply, before, redoText] { *redoText = mText; apply(before); },
      [apply, redoText] { apply(*redoText); },
      "Text edit"));
    return;
  }
  mUndo.push_back(mText);
  if (mUndo.size() > 100) {
    mUndo.erase(mUndo.begin());
  }
  mRedo.clear();
}

void TextInput::moveCursor(const size_t cursor, const bool extend) {
  if (extend) {
    if (!hasSelection()) {
      mSelectionAnchor = mCursor;
    }
  } else {
    mSelectionAnchor = std::min(cursor, mText.size());
  }
  mCursor = std::min(cursor, mText.size());
  mComposition.clear();
  ensureCursorVisible();
  setNeedsPaint();
}

size_t TextInput::indexAtX(const float x) const {
  if (mFont == nullptr) {
    return 0;
  }
  const float local = x - bounds().getLeft() - mPadding;
  const std::vector<size_t> boundaries = unicode::computeGraphemeBreaks(mText);
  size_t best = 0;
  float bestDistance = std::abs(local);
  float advance = 0.0f;
  size_t previous = 0;
  for (const size_t index: boundaries) {
    if (index == 0) continue;
    const std::string_view grapheme = std::string_view(mText).substr(previous, index - previous);
    advance += mFallback != nullptr ? mFallback->measure(grapheme) : mFont->measureWidth(grapheme);
    const float distance = std::abs(local - advance);
    if (distance < bestDistance) {
      bestDistance = distance;
      best = index;
    }
    previous = index;
  }
  return best;
}

float TextInput::xForIndex(const size_t index) const {
  if (mFont == nullptr) {
    return 0.0f;
  }
  if (mLayout.lines.empty()) ensureLayout(0.0f);
  if (!mLayout.lines.empty()) {
    const size_t lineIndex = lineIndexForCursor(index);
    const TextLine& line = mLayout.lines[std::min(lineIndex, mLayout.lines.size() - 1)];
    return xForIndexInLine(index, line);
  }
  const size_t clamped = std::min(index, mText.size());
  const std::string_view value = std::string_view(mText).substr(0, clamped);
  return mFallback != nullptr ? mFallback->measure(value) : mFont->measureWidth(value);
}

void TextInput::applyTheme(const Theme& theme) {
  if (!mPolicyExplicit) {
    LineBreakPolicy policy = theme.lineBreakPolicy;
    policy.overflowWrap = OverflowWrap::BreakWord;
    if (mPolicy != policy) {
      mPolicy = policy;
      mLayoutWidth = -1.0f;
      mPendingEdit.reset();
      setNeedsLayout();
    }
  }
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mForeground = theme.controls.foreground;
  mMuted = theme.controls.muted;
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void TextInput::notifyChanged() {
  mChanged.emit(mText);
}


void TextInput::setCaretBlinkInterval(const std::chrono::milliseconds interval) {
  mCaretBlinkInterval = std::max(interval, std::chrono::milliseconds{ 0 });
  mCaretBlinkStart = std::chrono::steady_clock::now();
  setNeedsPaint();
}

bool TextInput::caretVisibleAt(const std::chrono::steady_clock::time_point now) const {
  if (!mFocused) return false;
  if (mCaretBlinkInterval.count() <= 0) return true;
  // The preedit must stay readable while the IME is composing, so the caret does not blink then.
  if (!mComposition.empty()) return true;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    now - mCaretBlinkStart).count();
  const auto interval = mCaretBlinkInterval.count();
  return interval > 0 && (elapsed / interval) % 2 == 0;
}

void TextInput::onFrame(const std::chrono::steady_clock::time_point now) {
  if (mFocused) {
    const bool visible = caretVisibleAt(now);
    if (visible != mCaretVisibleCached) {
      mCaretVisibleCached = visible;
      setNeedsPaint();
    }
  }
  Layer::onFrame(now);
}
std::optional<AccessibleTextSelection> TextInput::accessibleTextSelection() const {
  // The control counts UTF-8 bytes; UIA counts UTF-16 code units. Both conversions live here so the
  // bridge can treat the offsets as opaque.
  const auto toUtf16 = [](const std::string& text, const std::size_t byteOffset) {
    int units = 0;
    std::size_t offset = 0;
    while (offset < byteOffset && offset < text.size()) {
      const unsigned char lead = static_cast<unsigned char>(text[offset]);
      int length = 1;
      if ((lead & 0x80u) == 0) {
        length = 1;
      } else if ((lead & 0xE0u) == 0xC0u) {
        length = 2;
      } else if ((lead & 0xF0u) == 0xE0u) {
        length = 3;
      } else if ((lead & 0xF8u) == 0xF0u) {
        length = 4;
      }
      offset += static_cast<std::size_t>(std::min<int>(length, static_cast<int>(text.size() - offset)));
      // Astral code points need a surrogate pair, so they occupy two UTF-16 units.
      units += length == 4 ? 2 : 1;
    }
    return units;
  };
  return AccessibleTextSelection{ toUtf16(mText, selectionStart()),
    toUtf16(mText, selectionEnd()) };
}

bool TextInput::performAccessibilityAction(const AccessibilityAction action, const std::string_view value) {
  if (action == AccessibilityAction::SetValue) {
    setText(std::string(value));
    return true;
  }
  if (action == AccessibilityAction::Select) {
    const std::string text(value);
    const std::size_t comma = text.find(',');
    if (comma == std::string::npos) return false;
    const auto parse = [](const std::string& part) -> std::optional<int> {
      try {
        return std::stoi(part);
      } catch (const std::exception&) {
        return std::nullopt;
      }
    };
    const std::optional<int> startUnits = parse(text.substr(0, comma));
    const std::optional<int> endUnits = parse(text.substr(comma + 1));
    if (!startUnits.has_value() || !endUnits.has_value()) return false;
    // Convert back: walk the text and stop when the UTF-16 count reaches the requested offset.
    const auto toByte = [this](const int units) {
      int counted = 0;
      std::size_t offset = 0;
      while (offset < mText.size() && counted < units) {
        const unsigned char lead = static_cast<unsigned char>(mText[offset]);
        int length = 1;
        if ((lead & 0xE0u) == 0xC0u) length = 2;
        else if ((lead & 0xF0u) == 0xE0u) length = 3;
        else if ((lead & 0xF8u) == 0xF0u) length = 4;
        offset += static_cast<std::size_t>(std::min<int>(length, static_cast<int>(mText.size() - offset)));
        counted += length == 4 ? 2 : 1;
      }
      return offset;
    };
    setSelection(toByte(std::max(0, *startUnits)), toByte(std::max(0, *endUnits)));
    return true;
  }
  return false;
}

} // namespace coral::ui
