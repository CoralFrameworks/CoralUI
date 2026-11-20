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

#include <ui/ListView.h>

#include "TextPainter.h"

#include <ui/Theme.h>

#include <graphics/Paint.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui {

class ListView::VirtualContent final : public Layer {
 public:
  explicit VirtualContent(ListView* owner) : mOwner(owner) {}

  Size measure(const Size& available) override {
    const float width = available.width > 0.0f ? available.width : 200.0f;
    const float height = static_cast<float>(mOwner->mItems.size()) *
      (mOwner->mRowHeight + 1.0f);
    setMeasuredSize({ width, height });
    return measuredSize();
  }

  void drawSelf(RenderContext& context) override {
    if (mOwner->mItems.empty() || mOwner->mFont == nullptr) return;
    auto& canvas = context.canvas();
    const float pitch = mOwner->mRowHeight + 1.0f;
    const float top = bounds().getTop();
    const float viewportTop = mOwner->bounds().getTop();
    const float viewportBottom = mOwner->bounds().getBottom();
    const int first = std::max(0, static_cast<int>(std::floor((viewportTop - top) / pitch)));
    const int last = std::min(static_cast<int>(mOwner->mItems.size()) - 1,
      static_cast<int>(std::ceil((viewportBottom - top) / pitch)));
    if (mTextPainter == nullptr) mTextPainter = std::make_unique<TextPainter>();
    for (int index = first; index <= last; ++index) {
      const float rowTop = top + static_cast<float>(index) * pitch;
      const Rect row = Rect::MakeXYWH(bounds().getLeft(), rowTop, bounds().getWidth(),
        mOwner->mRowHeight);
      graphics::Paint background;
      background.color = index == mOwner->mSelected ? mOwner->mSelectedColor : mOwner->mNormalColor;
      background.style = graphics::Paint::Style::kFill;
      canvas.drawRect(row, background);

      graphics::Paint text;
      text.color = mOwner->mTextColor;
      text.style = graphics::Paint::Style::kFill;
      const float baseline = rowTop + (mOwner->mRowHeight + mOwner->mFont->metrics().ascent -
        mOwner->mFont->metrics().descent) * 0.5f;
      mTextPainter->draw(context, mOwner->mItems[static_cast<size_t>(index)],
        row.getLeft() + 10.0f, baseline, *mOwner->mFont, text);
    }
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  bool onPointerDown(const PointerEvent& event) override {
    const float pitch = mOwner->mRowHeight + 1.0f;
    if (pitch <= 0.0f) return false;
    const int index = static_cast<int>(std::floor((event.position.y - bounds().getTop()) / pitch));
    if (index < 0 || index >= static_cast<int>(mOwner->mItems.size())) return false;
    mOwner->select(index);
    return true;
  }

  bool onKeyDown(const KeyEvent& event) override {
    if (mOwner->mItems.empty()) return false;
    if (event.key == Key::Space || event.key == Key::Enter) {
      mOwner->select(mOwner->mSelected >= 0 ? mOwner->mSelected : 0);
      return true;
    }
    if (event.key == Key::Down) {
      mOwner->select(std::min(static_cast<int>(mOwner->mItems.size()) - 1,
        mOwner->mSelected + 1));
      return true;
    }
    if (event.key == Key::Up) {
      mOwner->select(std::max(0, mOwner->mSelected - 1));
      return true;
    }
    return false;
  }

 private:
  ListView* mOwner = nullptr;
  std::unique_ptr<TextPainter> mTextPainter;
};

ListView::ListView(std::shared_ptr<const graphics::Font> font, const float rowHeight)
    : mFont(std::move(font)), mRowHeight(std::max(1.0f, rowHeight)) {
  setAccessibleRole(AccessibilityRole::List);
  mVirtual = std::make_shared<VirtualContent>(this);
  setContent(mVirtual);
}

ListView::~ListView() = default;

void ListView::addItem(std::string text) {
  mItems.push_back(std::move(text));
  if (mVirtual != nullptr) {
    mVirtual->setNeedsLayout();
    mVirtual->setNeedsPaint();
  }
  setNeedsLayout();
  setNeedsPaint();
}

void ListView::setItems(std::vector<std::string> items) {
  if (mItems == items) return;
  mItems = std::move(items);
  if (mSelected >= static_cast<int>(mItems.size())) mSelected = -1;
  if (mVirtual != nullptr) {
    mVirtual->setNeedsLayout();
    mVirtual->setNeedsPaint();
  }
  setNeedsLayout();
  setNeedsPaint();
}

void ListView::clear() {
  mItems.clear();
  mSelected = -1;
  if (mVirtual != nullptr) {
    mVirtual->setNeedsLayout();
    mVirtual->setNeedsPaint();
  }
  setNeedsLayout();
  setNeedsPaint();
}

void ListView::setSelectedIndex(const int index) {
  if (index < -1 || index >= static_cast<int>(mItems.size())) return;
  mSelected = index;
  if (mVirtual != nullptr) mVirtual->setNeedsPaint();
  setNeedsPaint();
}

void ListView::setOnSelection(std::function<void(int, const std::string&)> callback) {
  mOnSelection = std::move(callback);
}

void ListView::setRowColors(const graphics::Color normal, const graphics::Color selected) {
  mNormalColor = normal;
  mSelectedColor = selected;
  if (mVirtual != nullptr) mVirtual->setNeedsPaint();
  setNeedsPaint();
}

void ListView::applyTheme(const Theme& theme) {
  ScrollView::applyTheme(theme);
  mNormalColor = theme.surface;
  mSelectedColor = theme.controls.backgroundPressed;
  mTextColor = theme.controls.foreground;
  if (mVirtual != nullptr) mVirtual->setNeedsPaint();
  setNeedsPaint();
}

void ListView::select(const int index) {
  if (index < 0 || index >= static_cast<int>(mItems.size())) return;
  setSelectedIndex(index);
  ensureVisible(index);
  if (mOnSelection) mOnSelection(index, mItems[static_cast<size_t>(index)]);
}

void ListView::ensureVisible(const int index) {
  if (index < 0 || bounds().getHeight() <= 0.0f) return;
  const float pitch = mRowHeight + 1.0f;
  const float rowTop = static_cast<float>(index) * pitch;
  const float rowBottom = rowTop + mRowHeight;
  if (rowTop < scrollY()) {
    setScrollOffset(0.0f, rowTop);
  } else if (rowBottom > scrollY() + bounds().getHeight()) {
    setScrollOffset(0.0f, rowBottom - bounds().getHeight());
  }
}

} // namespace coral::ui
