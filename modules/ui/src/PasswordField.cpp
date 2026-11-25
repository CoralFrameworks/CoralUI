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

#include <ui/PasswordField.h>

#include "WidgetDrawing.h"

#include <ui/Clipboard.h>
#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {
std::string encodeUtf8(const char32_t codepoint) {
  std::string out;
  if (codepoint <= 0x7F) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return out;
}

size_t codepointCount(const std::string& text, const size_t end) {
  size_t count = 0;
  for (size_t index = 0; index < end && index < text.size(); ++index) {
    if ((static_cast<unsigned char>(text[index]) & 0xC0) != 0x80) ++count;
  }
  return count;
}
} // namespace

PasswordField::PasswordField(std::string placeholder, std::shared_ptr<const Font> font)
    : mPlaceholder(std::move(placeholder)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::TextField);
  setAccessibleName("Password");
  syncAccessibleValue();
}

void PasswordField::setText(std::string text) {
  mText = std::move(text);
  mComposition.clear();
  mCursor = mText.size();
  syncAccessibleValue();
  changed();
  setNeedsPaint();
}

void PasswordField::setPlaceholder(std::string placeholder) {
  mPlaceholder = std::move(placeholder);
  setNeedsPaint();
}

void PasswordField::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  setNeedsLayout();
  setNeedsPaint();
}

void PasswordField::setMaxLength(const size_t length) {
  mMaxLength = std::max<size_t>(1, length);
  if (codepointCount(mText, mText.size()) > mMaxLength) {
    while (!mText.empty() && codepointCount(mText, mText.size()) > mMaxLength) {
      mText.erase(previousBoundary(mText.size()));
    }
    mCursor = mText.size();
    syncAccessibleValue();
    changed();
  }
}

void PasswordField::setColors(const Color background, const Color border,
    const Color foreground, const Color focus) {
  mBackground = background;
  mBorder = border;
  mForeground = foreground;
  mFocus = focus;
  setNeedsPaint();
}

Size PasswordField::measure(const Size& available) {
  (void)available;
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 18.0f : 32.0f;
  setMeasuredSize({ 220.0f, std::max(32.0f, height) });
  return measuredSize();
}

void PasswordField::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), fill);
  graphics::Paint border;
  border.color = mFocused ? mFocus : mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);
  if (mFont == nullptr) return;
  if (mText.empty() && mComposition.empty()) {
    detail::drawLabel(context, mPlaceholder, bounds(), *mFont, mBorder,
      bounds().getLeft() + mPadding);
  } else {
    const std::string bullet = "•";
    const float width = mFont->measureWidth(bullet);
    const size_t total = codepointCount(mText, mText.size()) +
      codepointCount(mComposition, mComposition.size());
    for (size_t index = 0; index < total; ++index) {
      detail::drawLabel(context, bullet,
        Rect::MakeXYWH(bounds().getLeft() + mPadding + width * static_cast<float>(index),
          bounds().getTop(), width, bounds().getHeight()),
        *mFont, mForeground, bounds().getLeft() + mPadding + width * static_cast<float>(index));
    }
  }
  if (mFocused && mCaretVisible) {
    graphics::Paint caret;
    caret.color = mFocus;
    caret.style = graphics::Paint::Style::kFill;
    canvas.drawRect(Rect::MakeXYWH(caretX(), bounds().getTop() + 7.0f, 1.5f,
      bounds().getHeight() - 14.0f), caret);
  }
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

void PasswordField::onFrame(const std::chrono::steady_clock::time_point now) {
  Layer::onFrame(now);
  if (!mFocused) return;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    now - mCaretStart).count();
  const bool visible = (elapsed / 500) % 2 == 0;
  if (visible != mCaretVisible) {
    mCaretVisible = visible;
    setNeedsPaint();
  }
}

bool PasswordField::onPointerDown(const PointerEvent& event) {
  if (mFont == nullptr) return true;
  const float width = std::max(1.0f, mFont->measureWidth("•"));
  const int index = static_cast<int>(std::floor((event.position.x - bounds().getLeft() - mPadding) / width));
  size_t cursor = 0;
  int current = 0;
  while (cursor < mText.size() && current < index) {
    cursor = nextBoundary(cursor);
    ++current;
  }
  mCursor = cursor;
  setNeedsPaint();
  return true;
}

bool PasswordField::onKeyDown(const KeyEvent& event) {
  if ((event.meta || event.control) && event.key == Key::V) {
    paste();
    return true;
  }
  if ((event.meta || event.control) && (event.key == Key::C || event.key == Key::X)) {
    // Never publish the secret to the shared clipboard.
    return true;
  }
  if (event.key == Key::Backspace && mCursor > 0) {
    const size_t previous = previousBoundary(mCursor);
    mText.erase(previous, mCursor - previous);
    mCursor = previous;
    syncAccessibleValue();
    changed();
    setNeedsPaint();
    return true;
  }
  if (event.key == Key::Delete && mCursor < mText.size()) {
    mText.erase(mCursor, nextBoundary(mCursor) - mCursor);
    syncAccessibleValue();
    changed();
    setNeedsPaint();
    return true;
  }
  if (event.key == Key::Left && mCursor > 0) {
    mCursor = previousBoundary(mCursor);
    setNeedsPaint();
    return true;
  }
  if (event.key == Key::Right && mCursor < mText.size()) {
    mCursor = nextBoundary(mCursor);
    setNeedsPaint();
    return true;
  }
  if (event.key == Key::Home) {
    mCursor = 0;
    setNeedsPaint();
    return true;
  }
  if (event.key == Key::End) {
    mCursor = mText.size();
    setNeedsPaint();
    return true;
  }
  return false;
}

bool PasswordField::onTextInput(const char32_t character) {
  if (character < 0x20 || character == 0x7F) return false;
  if (codepointCount(mText, mText.size()) >= mMaxLength) return false;
  mComposition.clear();
  insertText(encodeUtf8(character));
  return true;
}

bool PasswordField::onTextComposition(const std::string& text) {
  mComposition = text;
  syncAccessibleValue();
  setNeedsPaint();
  return true;
}

bool PasswordField::onTextCommit(const std::string& text) {
  mComposition.clear();
  if (!text.empty()) insertText(text);
  syncAccessibleValue();
  setNeedsPaint();
  return true;
}

bool PasswordField::onTextDelete(const int32_t index, const uint32_t length) {
  if (length == 0) return true;
  const int64_t start = static_cast<int64_t>(mCursor) + index;
  const size_t begin = static_cast<size_t>(std::max<int64_t>(0, start));
  const size_t end = std::min(mText.size(), begin + length);
  if (begin >= end) return true;
  mText.erase(begin, end - begin);
  mCursor = begin;
  syncAccessibleValue();
  changed();
  setNeedsPaint();
  return true;
}

Rect PasswordField::imeCursorRect() const {
  return Rect::MakeXYWH(caretX(), bounds().getTop() + 7.0f, 2.0f,
    std::max(1.0f, bounds().getHeight() - 14.0f));
}

void PasswordField::onFocusChanged(const bool focused) {
  mFocused = focused;
  mCaretStart = std::chrono::steady_clock::now();
  mCaretVisible = focused;
  setNeedsPaint();
}

bool PasswordField::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (action != AccessibilityAction::SetValue) return false;
  setText(std::string(value));
  return true;
}

void PasswordField::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mFocus = theme.controls.borderFocused;
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsPaint();
}

void PasswordField::syncAccessibleValue() {
  std::string masked;
  const size_t total = codepointCount(mText, mText.size()) +
    codepointCount(mComposition, mComposition.size());
  for (size_t index = 0; index < total; ++index) {
    masked += "\xE2\x80\xA2";
  }
  setAccessibleValue(masked);
}

void PasswordField::paste() {
  std::string text = Clipboard::system()->getText();
  std::string clean;
  for (const char character: text) {
    if (static_cast<unsigned char>(character) >= 0x20 &&
        character != '\x7F') clean.push_back(character);
  }
  insertText(clean);
}

void PasswordField::changed() {
  if (mOnChanged) mOnChanged(mText);
}

size_t PasswordField::cursorCodepoints() const {
  return codepointCount(mText, mCursor);
}

size_t PasswordField::previousBoundary(size_t index) const {
  if (index == 0) return 0;
  --index;
  while (index > 0 && (static_cast<unsigned char>(mText[index]) & 0xC0) == 0x80) --index;
  return index;
}

size_t PasswordField::nextBoundary(size_t index) const {
  if (index >= mText.size()) return mText.size();
  ++index;
  while (index < mText.size() &&
         (static_cast<unsigned char>(mText[index]) & 0xC0) == 0x80) ++index;
  return index;
}

float PasswordField::caretX() const {
  if (mFont == nullptr) return bounds().getLeft() + mPadding;
  return bounds().getLeft() + mPadding +
    static_cast<float>(cursorCodepoints()) * mFont->measureWidth("•");
}

void PasswordField::insertText(const std::string& text) {
  mText.insert(mCursor, text);
  mCursor += text.size();
  syncAccessibleValue();
  changed();
  setNeedsPaint();
}

} // namespace coral::ui
