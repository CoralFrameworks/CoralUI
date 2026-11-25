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

#include <ui/MultiSelect.h>

#include "WidgetDrawing.h"

#include <ui/Menu.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

MultiSelect::MultiSelect(std::vector<std::string> options, std::shared_ptr<const Font> font)
    : mOptions(std::move(options)), mFont(std::move(font)) {
  setAccessibleRole(AccessibilityRole::ComboBox);
  rebuildMenu();
  setAccessibleValue(summary());
}

MultiSelect::~MultiSelect() {
  *mAlive = false;
  if (mMenu != nullptr) mMenu->hide();
  mMenu.reset();
}

void MultiSelect::setOptions(std::vector<std::string> options) {
  mOptions = std::move(options);
  std::vector<int> next;
  for (const int index: mSelected) {
    if (index >= 0 && index < static_cast<int>(mOptions.size())) next.push_back(index);
  }
  mSelected = std::move(next);
  rebuildMenu();
  setAccessibleValue(summary());
  setNeedsLayout();
  setNeedsPaint();
}

void MultiSelect::setSelectedIndices(std::vector<int> indices) {
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  indices.erase(std::remove_if(indices.begin(), indices.end(), [this](const int index) {
    return index < 0 || index >= static_cast<int>(mOptions.size());
  }), indices.end());
  if (indices == mSelected) return;
  mSelected = std::move(indices);
  setAccessibleValue(summary());
  setNeedsPaint();
  if (mOnChanged) mOnChanged(mSelected);
}

void MultiSelect::setPlaceholder(std::string placeholder) {
  mPlaceholder = std::move(placeholder);
  setNeedsPaint();
}

void MultiSelect::setFont(std::shared_ptr<const Font> font) {
  mFont = std::move(font);
  rebuildMenu();
  setNeedsLayout();
  setNeedsPaint();
}

void MultiSelect::setColors(const Color background, const Color border, const Color foreground,
    const Color muted, const Color accent) {
  mBackground = background;
  mBorder = border;
  mForeground = foreground;
  mMuted = muted;
  mFocus = accent;
  setNeedsPaint();
}

std::string MultiSelect::summary() const {
  if (mSelected.empty()) return mPlaceholder;
  std::string result;
  for (size_t index = 0; index < mSelected.size(); ++index) {
    if (index != 0) result += ", ";
    result += mOptions[static_cast<size_t>(mSelected[index])];
  }
  return result;
}

Size MultiSelect::measure(const Size& available) {
  (void)available;
  float width = 140.0f;
  if (mFont != nullptr) {
    width = std::max(width, mFont->measureWidth(summary()) + mPadding * 2.0f + 22.0f);
  }
  const float height = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 14.0f : 30.0f;
  setMeasuredSize({ width, std::max(28.0f, height) });
  return measuredSize();
}

void MultiSelect::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint fill;
  fill.color = mBackground;
  fill.style = graphics::Paint::Style::kFill;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), fill);
  graphics::Paint stroke;
  stroke.color = mFocused ? mFocus : mBorder;
  stroke.style = graphics::Paint::Style::kStroke;
  stroke.strokeWidth = 1.0f;
  canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), stroke);
  if (mFont != nullptr) {
    const Rect textBounds = Rect::MakeLTRB(bounds().getLeft() + mPadding, bounds().getTop(),
      bounds().getRight() - 24.0f, bounds().getBottom());
    detail::drawLabel(context, summary(), textBounds, *mFont,
      mSelected.empty() ? mMuted : mForeground, textBounds.getLeft());
  }
  graphics::Paint chevron = stroke;
  chevron.strokeWidth = 1.5f;
  const float cx = bounds().getRight() - 14.0f;
  const float cy = bounds().getTop() + bounds().getHeight() * 0.5f;
  canvas.drawLine({ cx - 4.0f, cy - 2.0f }, { cx, cy + 2.0f }, chevron);
  canvas.drawLine({ cx, cy + 2.0f }, { cx + 4.0f, cy - 2.0f }, chevron);
  detail::drawFocusRing(canvas, bounds(), mFocused, mFocus, mRadius, focusRingWidth());
}

bool MultiSelect::onPointerDown(const PointerEvent&) {
  openMenu();
  return true;
}

bool MultiSelect::onKeyDown(const KeyEvent& event) {
  if (event.key == Key::Up || event.key == Key::Down ||
      event.key == Key::Home || event.key == Key::End) {
    openMenu();
    if (mMenu != nullptr && mMenu->isOpen()) mMenu->onKeyDown(event);
    return true;
  }
  if (event.key == Key::Space || event.key == Key::Enter) {
    openMenu();
    return true;
  }
  return false;
}

void MultiSelect::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

bool MultiSelect::performAccessibilityAction(const AccessibilityAction action, std::string_view) {
  if (action != AccessibilityAction::Invoke && action != AccessibilityAction::Select) return false;
  openMenu();
  return true;
}

void MultiSelect::applyTheme(const Theme& theme) {
  mBackground = theme.controls.background;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mMuted = theme.controls.border;
  mFocus = theme.controls.borderFocused;
  mPadding = theme.spacing.sm;
  mRadius = theme.radius.md;
  setNeedsLayout();
  setNeedsPaint();
}

void MultiSelect::rebuildMenu() {
  mMenu = std::make_shared<Menu>(mFont);
  for (size_t index = 0; index < mOptions.size(); ++index) {
    const bool checked = std::find(mSelected.begin(), mSelected.end(), static_cast<int>(index)) !=
      mSelected.end();
    const auto alive = mAlive;
    mMenu->addCheckItem(mOptions[index], checked, [this, alive, index](bool on) {
      if (!*alive) return;
      if (on) {
        // A menu can deliver a check callback more than once (for example while a menu is being
        // rebuilt); the selected set is a set, not a bag.
        if (std::find(mSelected.begin(), mSelected.end(), static_cast<int>(index)) ==
            mSelected.end()) {
          mSelected.push_back(static_cast<int>(index));
          std::sort(mSelected.begin(), mSelected.end());
        }
      } else {
        mSelected.erase(std::remove(mSelected.begin(), mSelected.end(),
          static_cast<int>(index)), mSelected.end());
      }
      setAccessibleValue(summary());
      setNeedsPaint();
      if (mOnChanged) mOnChanged(mSelected);
    });
  }
}

void MultiSelect::openMenu() {
  if (mOptions.empty() || mMenu == nullptr || window() == nullptr || window()->root() == nullptr) {
    return;
  }
  if (mMenu->parent() != window()->root().get()) {
    if (mMenu->parent() != nullptr) mMenu->parent()->removeChild(mMenu);
    window()->root()->addChild(mMenu);
  }
  mMenu->showAt(boundsInWindow());
}

} // namespace coral::ui
