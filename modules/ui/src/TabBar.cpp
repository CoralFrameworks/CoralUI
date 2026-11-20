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

#include <ui/TabBar.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

TabBar::TabBar(std::shared_ptr<const graphics::Font> font) : mFont(std::move(font)) {}

TabBar::~TabBar() = default;

int TabBar::addTab(std::string label, std::shared_ptr<Layer> content) {
  Tab tab;
  tab.label = std::move(label);
  tab.content = std::move(content);
  if (tab.content != nullptr) {
    addChild(tab.content);
  }
  mTabs.push_back(std::move(tab));
  const int index = static_cast<int>(mTabs.size()) - 1;
  if (mActive < 0) {
    mActive = index;
  }
  updateVisibility();
  setNeedsLayout();
  setNeedsPaint();
  return index;
}

void TabBar::clear() {
  for (const Tab& tab: mTabs) {
    if (tab.content != nullptr) {
      removeChild(tab.content);
    }
  }
  mTabs.clear();
  mActive = -1;
  setNeedsLayout();
  setNeedsPaint();
}

void TabBar::setActiveTab(const int index) {
  if (mTabs.empty()) {
    mActive = -1;
    return;
  }
  const int next = std::clamp(index, 0, static_cast<int>(mTabs.size()) - 1);
  if (next == mActive) {
    return;
  }
  mActive = next;
  updateVisibility();
  setNeedsLayout();
  setNeedsPaint();
  if (mOnChanged) {
    mOnChanged(mActive);
  }
}

std::shared_ptr<Layer> TabBar::content(const int index) const {
  if (index < 0 || index >= static_cast<int>(mTabs.size())) {
    return nullptr;
  }
  return mTabs[static_cast<size_t>(index)].content;
}

float TabBar::tabWidth(const std::string& label) const {
  return (mFont != nullptr ? mFont->measureWidth(label) : 60.0f) + mTabPadding * 2.0f;
}

Size TabBar::measure(const Size& available) {
  float headerWidth = 0.0f;
  for (const Tab& tab: mTabs) {
    headerWidth += tabWidth(tab.label);
  }
  const float headerHeight = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + 20.0f
    : 36.0f;
  mHeaderHeight = std::max(28.0f, headerHeight);

  float contentHeight = 0.0f;
  if (mActive >= 0 && mTabs[static_cast<size_t>(mActive)].content != nullptr) {
    const Size contentSize = mTabs[static_cast<size_t>(mActive)].content->measure({
      available.width, std::max(0.0f, available.height - mHeaderHeight) });
    contentHeight = contentSize.height;
  }
  const float width = std::max(available.width, headerWidth);
  setMeasuredSize({ width, mHeaderHeight + contentHeight });
  return measuredSize();
}

void TabBar::layout() {
  if (mTabs.empty()) {
    clearNeedsLayout();
    return;
  }
  float left = bounds().getLeft();
  for (Tab& tab: mTabs) {
    const float width = tabWidth(tab.label);
    tab.bounds = Rect::MakeXYWH(left, bounds().getTop(), width, mHeaderHeight);
    left += width + mGap;
  }

  const Rect contentBounds = Rect::MakeXYWH(bounds().getLeft(),
    bounds().getTop() + mHeaderHeight, bounds().getWidth(),
    std::max(0.0f, bounds().getHeight() - mHeaderHeight));
  for (size_t index = 0; index < mTabs.size(); ++index) {
    if (mTabs[index].content != nullptr) {
      mTabs[index].content->setBounds(contentBounds);
      if (static_cast<int>(index) == mActive) {
        mTabs[index].content->layout();
      }
    }
  }
  clearNeedsLayout();
}

void TabBar::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  graphics::Paint background;
  background.color = mBackground;
  background.style = graphics::Paint::Style::kFill;
  canvas.drawRect(bounds(), background);

  for (size_t index = 0; index < mTabs.size(); ++index) {
    const Tab& tab = mTabs[index];
    graphics::Paint tabPaint;
    tabPaint.color = static_cast<int>(index) == mActive ? mActiveBackground : mTabBackground;
    tabPaint.style = graphics::Paint::Style::kFill;
    canvas.drawRect(tab.bounds, tabPaint);
    if (mFont != nullptr) {
      detail::drawLabel(context, tab.label, tab.bounds, *mFont, mForeground,
        tab.bounds.getLeft() + mTabPadding);
    }
    if (static_cast<int>(index) == mActive) {
      graphics::Paint underline;
      underline.color = mAccent;
      underline.style = graphics::Paint::Style::kFill;
      canvas.drawRect(Rect::MakeXYWH(tab.bounds.getLeft(), tab.bounds.getBottom() - 2.0f,
        tab.bounds.getWidth(), 2.0f), underline);
    }
  }

  graphics::Paint border;
  border.color = mBorder;
  border.style = graphics::Paint::Style::kFill;
  canvas.drawRect(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + mHeaderHeight - 1.0f,
    bounds().getWidth(), 1.0f), border);
  detail::drawFocusRing(canvas, bounds(), mFocused, mAccent, mRadius, focusRingWidth());
}

int TabBar::tabAt(const Point& point) const {
  for (size_t index = 0; index < mTabs.size(); ++index) {
    if (mTabs[index].bounds.contains(point)) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

bool TabBar::onPointerDown(const PointerEvent& event) {
  if (event.position.y > bounds().getTop() + mHeaderHeight) {
    return false;
  }
  const int index = tabAt(event.position);
  if (index >= 0) {
    setActiveTab(index);
  }
  return true;
}

bool TabBar::onKeyDown(const KeyEvent& event) {
  if (mTabs.empty()) {
    return false;
  }
  if (event.key == Key::Left) {
    setActiveTab(mActive <= 0 ? static_cast<int>(mTabs.size()) - 1 : mActive - 1);
    return true;
  }
  if (event.key == Key::Right) {
    setActiveTab((mActive + 1) % static_cast<int>(mTabs.size()));
    return true;
  }
  return false;
}

void TabBar::onFocusChanged(const bool focused) {
  mFocused = focused;
  setNeedsPaint();
}

void TabBar::applyTheme(const Theme& theme) {
  mBackground = theme.surface;
  mTabBackground = theme.background;
  mActiveBackground = theme.surfaceRaised;
  mBorder = theme.controls.border;
  mForeground = theme.controls.foreground;
  mAccent = theme.controls.accent;
  mTabPadding = theme.spacing.md;
  mRadius = theme.radius.md;
  mHeaderHeight = mFont != nullptr
    ? mFont->metrics().ascent + mFont->metrics().descent + theme.spacing.lg + 4.0f
    : 36.0f;
  setNeedsLayout();
  setNeedsPaint();
}

void TabBar::updateVisibility() {
  for (size_t index = 0; index < mTabs.size(); ++index) {
    if (mTabs[index].content != nullptr) {
      mTabs[index].content->setVisible(static_cast<int>(index) == mActive);
    }
  }
}

} // namespace coral::ui
