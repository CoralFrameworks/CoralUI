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

#include <ui/TreeView.h>

#include "TextPainter.h"

#include <ui/Theme.h>

#include <graphics/Image.h>
#include <graphics/Paint.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace coral::ui {

class TreeVirtualContent final : public Layer {
 public:
  explicit TreeVirtualContent(TreeView* owner) : mOwner(owner) {}

  Size measure(const Size& available) override {
    const float width = available.width > 0.0f ? available.width : 200.0f;
    const float height = static_cast<float>(mOwner->mRows.size()) * pitch();
    setMeasuredSize({ width, std::max(1.0f, height) });
    return measuredSize();
  }

  void drawSelf(RenderContext& context) override {
    if (mOwner->mRows.empty() || mOwner->mFont == nullptr) return;
    auto& canvas = context.canvas();
    const float rowPitch = pitch();
    const float top = bounds().getTop();
    const float viewportTop = mOwner->bounds().getTop();
    const float viewportBottom = mOwner->bounds().getBottom();
    const int first = std::max(0, static_cast<int>(std::floor((viewportTop - top) / rowPitch)));
    const int last = std::min(static_cast<int>(mOwner->mRows.size()) - 1,
      static_cast<int>(std::ceil((viewportBottom - top) / rowPitch)));
    if (first > last) return;
    if (mLabelPainter == nullptr) mLabelPainter = std::make_unique<TextPainter>();
    if (mArrowPainter == nullptr) mArrowPainter = std::make_unique<TextPainter>();
    if (mDetailPainter == nullptr) mDetailPainter = std::make_unique<TextPainter>();

    for (int index = first; index <= last; ++index) {
      const TreeView::Row& row = mOwner->mRows[static_cast<size_t>(index)];
      if (row.id < 0 || row.id >= static_cast<int>(mOwner->mNodes.size())) continue;
      const TreeView::Node& node = mOwner->mNodes[static_cast<size_t>(row.id)];
      const float rowTop = top + static_cast<float>(index) * rowPitch;
      const Rect rowRect = Rect::MakeXYWH(bounds().getLeft(), rowTop, bounds().getWidth(),
        mOwner->mRowHeight);
      graphics::Paint background;
      background.color = row.id == mOwner->mSelected ? mOwner->mSelectedColor : mOwner->mNormalColor;
      background.style = graphics::Paint::Style::kFill;
      canvas.drawRect(rowRect, background);

      const float indent = 8.0f + static_cast<float>(row.depth) * 14.0f;
      const float baseline = rowTop + (mOwner->mRowHeight + mOwner->mFont->metrics().ascent -
        mOwner->mFont->metrics().descent) * 0.5f;
      if (node.folder) {
        graphics::Paint arrow;
        arrow.color = mOwner->mMutedColor;
        arrow.style = graphics::Paint::Style::kFill;
        mArrowPainter->draw(context, node.expanded ? "v" : ">", rowRect.getLeft() + indent,
          baseline, *mOwner->mFont, arrow);
      }
      float labelX = rowRect.getLeft() + indent + 16.0f;
      if (node.icon != nullptr) {
        const graphics::Bitmap& bitmap = node.icon->peekBitmap();
        if (!bitmap.isEmpty()) {
          graphics::Paint iconPaint;
          iconPaint.color = graphics::Color::White();
          iconPaint.style = graphics::Paint::Style::kFill;
          canvas.drawBitmapRect(bitmap,
            Rect::MakeWH(static_cast<float>(bitmap.width()), static_cast<float>(bitmap.height())),
            Rect::MakeXYWH(labelX, rowTop + 5.0f, 18.0f, 18.0f), iconPaint);
          labelX += 22.0f;
        }
      }
      graphics::Paint text;
      text.color = mOwner->mTextColor;
      text.style = graphics::Paint::Style::kFill;
      mLabelPainter->draw(context, node.label, labelX, baseline, *mOwner->mFont, text);
      if (!node.detail.empty()) {
        const float detailWidth = mOwner->mFont->measureWidth(node.detail);
        const float detailX = rowRect.getRight() - 8.0f - detailWidth;
        if (detailX > labelX + 12.0f) {
          graphics::Paint detail;
          detail.color = mOwner->mMutedColor;
          detail.style = graphics::Paint::Style::kFill;
          mDetailPainter->draw(context, node.detail, detailX, baseline, *mOwner->mFont, detail);
        }
      }
    }
  }

  [[nodiscard]] bool focusable() const override {
    return true;
  }

  bool onPointerDown(const PointerEvent& event) override {
    const int index = rowAt(event.position.y);
    if (index < 0 || index >= static_cast<int>(mOwner->mRows.size())) return false;
    const TreeView::Row& row = mOwner->mRows[static_cast<size_t>(index)];
    const TreeView::Node& node = mOwner->mNodes[static_cast<size_t>(row.id)];
    const float indent = 8.0f + static_cast<float>(row.depth) * 14.0f;
    if (node.folder && event.position.x < bounds().getLeft() + indent + 16.0f) {
      mOwner->toggle(row.id);
    } else {
      mOwner->select(row.id);
    }
    return true;
  }

  bool onKeyDown(const KeyEvent& event) override {
    if (mOwner->mRows.empty()) return false;
    int current = -1;
    for (size_t index = 0; index < mOwner->mRows.size(); ++index) {
      if (mOwner->mRows[index].id == mOwner->mSelected) {
        current = static_cast<int>(index);
        break;
      }
    }
    if (event.key == Key::Down) {
      mOwner->select(mOwner->mRows[static_cast<size_t>(
        std::min(static_cast<int>(mOwner->mRows.size()) - 1,
          (current < 0 ? 0 : current + 1)))].id);
      return true;
    }
    if (event.key == Key::Up) {
      mOwner->select(mOwner->mRows[static_cast<size_t>(std::max(0,
        current < 0 ? 0 : current - 1))].id);
      return true;
    }
    if (current < 0) return false;
    const TreeView::Row& row = mOwner->mRows[static_cast<size_t>(current)];
    const TreeView::Node& node = mOwner->mNodes[static_cast<size_t>(row.id)];
    if (event.key == Key::Enter || event.key == Key::Space) {
      mOwner->activate(row.id);
      return true;
    }
    if (node.folder && event.key == Key::Right) {
      mOwner->setExpanded(row.id, true);
      return true;
    }
    if (node.folder && event.key == Key::Left) {
      mOwner->setExpanded(row.id, false);
      return true;
    }
    return false;
  }

 private:
  [[nodiscard]] float pitch() const {
    return mOwner->mRowHeight + 1.0f;
  }

  [[nodiscard]] int rowAt(const float y) const {
    const float rowPitch = pitch();
    if (rowPitch <= 0.0f) return -1;
    return static_cast<int>(std::floor((y - bounds().getTop()) / rowPitch));
  }

  TreeView* mOwner = nullptr;
  std::unique_ptr<TextPainter> mLabelPainter;
  std::unique_ptr<TextPainter> mArrowPainter;
  std::unique_ptr<TextPainter> mDetailPainter;
};

TreeView::TreeView(std::shared_ptr<const graphics::Font> font, const float rowHeight)
    : mFont(std::move(font)), mRowHeight(std::max(1.0f, rowHeight)) {
  setAccessibleRole(AccessibilityRole::Tree);
  mVirtual = std::make_shared<TreeVirtualContent>(this);
  setContent(mVirtual);
}

TreeView::~TreeView() = default;

int TreeView::addRoot(std::string label, const bool expanded) {
  Node node;
  node.label = std::move(label);
  node.folder = true;
  node.expanded = expanded;
  mNodes.push_back(std::move(node));
  rebuildRows();
  return static_cast<int>(mNodes.size()) - 1;
}

int TreeView::addChild(const int parent, std::string label, const bool folder) {
  if (parent < 0 || parent >= static_cast<int>(mNodes.size())) return -1;
  Node node;
  node.label = std::move(label);
  node.parent = parent;
  node.folder = folder;
  mNodes.push_back(std::move(node));
  const int id = static_cast<int>(mNodes.size()) - 1;
  mNodes[static_cast<size_t>(parent)].children.push_back(id);
  mNodes[static_cast<size_t>(parent)].folder = true;
  rebuildRows();
  return id;
}

void TreeView::clear() {
  mRows.clear();
  mNodes.clear();
  mSelected = -1;
  if (mVirtual != nullptr) {
    mVirtual->setNeedsLayout();
    mVirtual->setNeedsPaint();
  }
  setNeedsLayout();
  setNeedsPaint();
}

void TreeView::setExpanded(const int id, const bool expanded) {
  if (id < 0 || id >= static_cast<int>(mNodes.size())) return;
  Node& node = mNodes[static_cast<size_t>(id)];
  if (!node.folder || node.expanded == expanded) return;
  node.expanded = expanded;
  rebuildRows();
}

void TreeView::setSelected(const int id) {
  if (id < -1 || id >= static_cast<int>(mNodes.size())) return;
  mSelected = id;
  if (mVirtual != nullptr) mVirtual->setNeedsPaint();
  for (size_t index = 0; index < mRows.size(); ++index) {
    if (mRows[index].id == id) {
      ensureVisible(static_cast<int>(index));
      break;
    }
  }
  setNeedsPaint();
}

void TreeView::setNodeIcon(const int id, std::shared_ptr<const graphics::Image> icon) {
  if (id < 0 || id >= static_cast<int>(mNodes.size())) return;
  mNodes[static_cast<size_t>(id)].icon = std::move(icon);
  if (mVirtual != nullptr) mVirtual->setNeedsPaint();
}

void TreeView::setNodeDetail(const int id, std::string detail) {
  if (id < 0 || id >= static_cast<int>(mNodes.size())) return;
  mNodes[static_cast<size_t>(id)].detail = std::move(detail);
  if (mVirtual != nullptr) mVirtual->setNeedsPaint();
}

std::string TreeView::label(const int id) const {
  if (id < 0 || id >= static_cast<int>(mNodes.size())) return {};
  return mNodes[static_cast<size_t>(id)].label;
}

bool TreeView::isFolder(const int id) const {
  if (id < 0 || id >= static_cast<int>(mNodes.size())) return false;
  return mNodes[static_cast<size_t>(id)].folder;
}

void TreeView::setOnSelection(std::function<void(int, const std::string&)> callback) {
  mOnSelection = std::move(callback);
}

void TreeView::setOnActivated(std::function<void(int, const std::string&)> callback) {
  mOnActivated = std::move(callback);
}

void TreeView::applyTheme(const Theme& theme) {
  ScrollView::applyTheme(theme);
  mNormalColor = theme.surface;
  mSelectedColor = theme.controls.backgroundPressed;
  mTextColor = theme.controls.foreground;
  mMutedColor = theme.textMuted;
  if (mVirtual != nullptr) mVirtual->setNeedsPaint();
  setNeedsPaint();
}

bool TreeView::visible(const int id) const {
  for (int current = mNodes[static_cast<size_t>(id)].parent; current >= 0;
       current = mNodes[static_cast<size_t>(current)].parent) {
    if (!mNodes[static_cast<size_t>(current)].expanded) return false;
  }
  return true;
}

void TreeView::rebuildRows() {
  mRows.clear();
  std::function<void(int, int)> append = [&](const int id, const int depth) {
    if (!visible(id)) return;
    mRows.push_back({ id, depth });
    if (mNodes[static_cast<size_t>(id)].expanded) {
      for (const int child: mNodes[static_cast<size_t>(id)].children) append(child, depth + 1);
    }
  };
  for (int id = 0; id < static_cast<int>(mNodes.size()); ++id) {
    if (mNodes[static_cast<size_t>(id)].parent < 0) append(id, 0);
  }
  if (mVirtual != nullptr) {
    mVirtual->setNeedsLayout();
    mVirtual->setNeedsPaint();
  }
  setNeedsLayout();
  setNeedsPaint();
}

void TreeView::toggle(const int id) {
  if (id < 0 || id >= static_cast<int>(mNodes.size())) return;
  Node& node = mNodes[static_cast<size_t>(id)];
  if (!node.folder) return;
  node.expanded = !node.expanded;
  rebuildRows();
}

void TreeView::select(const int id) {
  if (id < 0 || id >= static_cast<int>(mNodes.size())) return;
  setSelected(id);
  if (mOnSelection) mOnSelection(id, mNodes[static_cast<size_t>(id)].label);
}

void TreeView::activate(const int id) {
  if (id < 0 || id >= static_cast<int>(mNodes.size())) return;
  if (mNodes[static_cast<size_t>(id)].folder) {
    toggle(id);
    return;
  }
  if (mOnActivated) mOnActivated(id, mNodes[static_cast<size_t>(id)].label);
}

void TreeView::ensureVisible(const int row) {
  if (row < 0 || bounds().getHeight() <= 0.0f) return;
  const float pitch = mRowHeight + 1.0f;
  const float rowTop = static_cast<float>(row) * pitch;
  const float rowBottom = rowTop + mRowHeight;
  if (rowTop < scrollY()) {
    setScrollOffset(0.0f, rowTop);
  } else if (rowBottom > scrollY() + bounds().getHeight()) {
    setScrollOffset(0.0f, rowBottom - bounds().getHeight());
  }
}

} // namespace coral::ui
