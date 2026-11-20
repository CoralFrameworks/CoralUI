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

#include <ui/Dialog.h>

#include "TextPainter.h"

#include <ui/Button.h>
#include <ui/Theme.h>

#include <graphics/Paint.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {
constexpr float kPanelWidth = 420.0f;
constexpr float kButtonGap = 8.0f;
} // namespace

Dialog::Dialog(std::string title, std::shared_ptr<const graphics::Font> font)
    : mFont(std::move(font)), mTitle(std::move(title)) {
  setVisible(false);
  // A shown dialog is modal for the keyboard too: Tab stays between its buttons and a click on the
  // dimmed area does not move focus to the page behind it.
  setFocusScope(true);
  setAccessibleRole(AccessibilityRole::Dialog);
}

Dialog::~Dialog() = default;

void Dialog::setTitle(std::string title) {
  mTitle = std::move(title);
  setNeedsPaint();
}

void Dialog::setMessage(std::string message) {
  mMessage = std::move(message);
  setNeedsPaint();
}

int Dialog::addButton(std::string label, std::function<void()> action) {
  const int index = static_cast<int>(mButtons.size());
  auto button = std::make_shared<Button>(std::move(label), mFont);
  button->setOnClick([this, index] { activate(index); });
  mButtons.push_back(button);
  mButtonActions.push_back(std::move(action));
  addChild(button);
  setNeedsLayout();
  return index;
}

void Dialog::setContent(std::shared_ptr<Layer> content) {
  if (mContent != nullptr) removeChild(mContent);
  mContent = std::move(content);
  if (mContent != nullptr) addChild(mContent);
  setNeedsLayout();
  setNeedsPaint();
}

void Dialog::setDefaultButton(const int index) {
  mDefaultButton = index >= 0 && index < static_cast<int>(mButtons.size()) ? index : -1;
}

void Dialog::setOnResult(std::function<void(int)> callback) {
  mOnResult = std::move(callback);
}

void Dialog::close(const int result) {
  mLastResult = result;
  hide();
  if (mOnResult) mOnResult(result);
}

void Dialog::activate(const int index) {
  if (index < 0 || index >= static_cast<int>(mButtons.size())) return;
  const std::function<void()> action = index < static_cast<int>(mButtonActions.size())
                                         ? mButtonActions[static_cast<std::size_t>(index)]
                                         : std::function<void()>{};
  close(index);
  if (action) action();
}

bool Dialog::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Escape) {
    close(-1);
    return true;
  }
  if (event.key == Key::Enter && mDefaultButton >= 0) {
    activate(mDefaultButton);
    return true;
  }
  return false;
}

void Dialog::show() {
  setVisible(true);
  if (bounds().isEmpty()) {
    setBounds(Rect::MakeWH(640.0f, 480.0f));
  }
  measure({ bounds().getWidth(), bounds().getHeight() });
  layout();
}

void Dialog::hide() {
  setVisible(false);
}

Size Dialog::measure(const Size& available) {
  (void)available;
  setMeasuredSize({ bounds().getWidth(), bounds().getHeight() });
  return measuredSize();
}

Rect Dialog::panelBounds() const {
  const float width = std::min(kPanelWidth, std::max(120.0f, bounds().getWidth() - 32.0f));
  const float panelHeight = std::min(mPanelHeight, std::max(120.0f, bounds().getHeight() - 40.0f));
  return Rect::MakeXYWH(bounds().getLeft() + (bounds().getWidth() - width) * 0.5f,
    bounds().getTop() + (bounds().getHeight() - panelHeight) * 0.5f, width, panelHeight);
}

void Dialog::layout() {
  const float width = std::min(kPanelWidth, std::max(120.0f, bounds().getWidth() - 32.0f));
  float contentHeight = 0.0f;
  if (mContent != nullptr) {
    contentHeight = mContent->measure({ width - mPanelPadding * 2.0f, 0.0f }).height;
  }
  // Title + message + content + button row: the panel grows with the content slot instead of
  // clipping it into a fixed 240pt box.
  mPanelHeight = std::max(140.0f, mPanelPadding * 2.0f + 30.0f + 30.0f + 8.0f + contentHeight +
                                    32.0f + 8.0f);
  const Rect panel = panelBounds();
  float right = panel.getRight() - mPanelPadding;
  const float buttonTop = panel.getBottom() - mPanelPadding - 32.0f;
  for (auto button = mButtons.rbegin(); button != mButtons.rend(); ++button) {
    const Size size = (*button)->measure({});
    const float buttonWidth = std::max(size.width, 80.0f);
    (*button)->setBounds(Rect::MakeXYWH(right - buttonWidth, buttonTop, buttonWidth, 32.0f));
    right -= buttonWidth + kButtonGap;
  }
  if (mContent != nullptr) {
    const float contentTop = panel.getTop() + mPanelPadding + 68.0f;
    const float contentBottom = buttonTop - 8.0f;
    mContent->setBounds(Rect::MakeXYWH(panel.getLeft() + mPanelPadding, contentTop,
      std::max(0.0f, panel.getWidth() - mPanelPadding * 2.0f),
      std::max(0.0f, contentBottom - contentTop)));
    mContent->layout();
  }
  clearNeedsLayout();
}

void Dialog::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  // When the dialog is hosted by the window overlay, the host draws the scrim; drawing a second one
  // would double the dimming and make the panel edges darker than the theme says.
  const bool hosted = parent() != nullptr && parent()->isOverlay();
  if (!hosted) {
    graphics::Paint dim;
    dim.color = mOverlay;
    dim.style = graphics::Paint::Style::kFill;
    canvas.drawRect(bounds(), dim);
  }

  const Rect panel = panelBounds();
  graphics::Paint surface;
  surface.color = mPanel;
  surface.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(panel, mRadius), surface);
  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kStroke;
  border.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(panel, mRadius), border);

  if (mFont == nullptr) {
    return;
  }
  graphics::Paint titlePaint;
  titlePaint.color = mTitleColor;
  titlePaint.style = graphics::Paint::Style::kFill;
  if (mTitlePainter == nullptr) mTitlePainter = std::make_unique<TextPainter>();
  mTitlePainter->draw(context, mTitle, panel.getLeft() + mPanelPadding,
    panel.getTop() + mPanelPadding + mFont->metrics().ascent, *mFont, titlePaint);
  graphics::Paint messagePaint = titlePaint;
  messagePaint.color = mMessageColor;
  if (mMessagePainter == nullptr) mMessagePainter = std::make_unique<TextPainter>();
  mMessagePainter->draw(context, mMessage, panel.getLeft() + mPanelPadding,
    panel.getTop() + mPanelPadding + 30.0f + mFont->metrics().ascent, *mFont, messagePaint);

  drawChildren(context);
}

void Dialog::applyTheme(const Theme& theme) {
  mPanel = theme.surface;
  mBorder = theme.controls.border;
  mTitleColor = theme.controls.foreground;
  mMessageColor = theme.controls.muted;
  mOverlay = theme.scrim;
  mPanelPadding = theme.spacing.lg;
  mRadius = theme.radius.lg;
  setNeedsLayout();
  setNeedsPaint();
}

bool Dialog::onPointerDown(const PointerEvent& event) {
  // The dimmed area blocks the click from reaching the layers behind the dialog.
  (void)event;
  return true;
}

} // namespace coral::ui
