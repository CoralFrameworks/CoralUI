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

#include <ui/DockView.h>

#include "WidgetDrawing.h"

#include <ui/Theme.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

namespace {
constexpr float kDragSlop = 4.0f;
} // namespace

/**
 * The independent layer behind a floating dock.
 *
 * The header is the drag handle; the content keeps normal hit testing below it. The window captures
 * the pointer for the header drag, so the panel can move outside its own original rect.
 */
class DockView::FloatingPanelLayer final : public Layer {
 public:
  FloatingPanelLayer(std::string title, std::shared_ptr<Layer> content,
      std::shared_ptr<const graphics::Font> font, const float headerHeight)
      : mTitle(std::move(title)), mContent(std::move(content)), mFont(std::move(font)),
        mHeaderHeight(headerHeight) {
    if (mContent != nullptr) {
      addChild(mContent);
    }
    setZIndex(1000);
  }

  void setFont(std::shared_ptr<const graphics::Font> font) {
    mFont = std::move(font);
    setNeedsLayout();
    setNeedsPaint();
  }

  [[nodiscard]] const std::shared_ptr<Layer>& content() const noexcept {
    return mContent;
  }

  [[nodiscard]] bool capturesPointerOnPress() const override {
    return true;
  }

  Size measure(const Size& available) override {
    const Size contentSize = mContent != nullptr ? mContent->measure(available) : Size{};
    setMeasuredSize({ std::max(120.0f, contentSize.width), mHeaderHeight + contentSize.height });
    return measuredSize();
  }

  void layout() override {
    if (mContent != nullptr) {
      mContent->setBounds(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop() + mHeaderHeight,
        bounds().getWidth(), std::max(0.0f, bounds().getHeight() - mHeaderHeight)));
      mContent->layout();
    }
    clearNeedsLayout();
  }

  void drawSelf(RenderContext& context) override {
    auto& canvas = context.canvas();
    graphics::Paint paint;
    paint.color = mBackground;
    paint.style = graphics::Paint::Style::kFill;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), paint);
    graphics::Paint header;
    header.color = mHeader;
    header.style = graphics::Paint::Style::kFill;
    canvas.drawRect(Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), bounds().getWidth(),
      mHeaderHeight), header);
    graphics::Paint border;
    border.color = mBorder;
    border.style = graphics::Paint::Style::kStroke;
    border.strokeWidth = 1.0f;
    canvas.drawRoundRect(graphics::RoundRect::MakeRectRadius(bounds(), mRadius), border);
    if (mFont != nullptr) {
      detail::drawLabel(context, mTitle,
        Rect::MakeXYWH(bounds().getLeft(), bounds().getTop(), bounds().getWidth(), mHeaderHeight),
        *mFont, mForeground, bounds().getLeft() + 10.0f);
    }
  }

  bool onPointerDown(const PointerEvent& event) override {
    if (event.position.y > bounds().getTop() + mHeaderHeight) {
      return false;
    }
    mDragging = true;
    mDragOffset = { event.position.x - bounds().getLeft(),
      event.position.y - bounds().getTop() };
    return true;
  }

  bool onPointerMove(const PointerEvent& event) override {
    if (!mDragging) {
      return false;
    }
    setBounds(Rect::MakeXYWH(event.position.x - mDragOffset.x,
      event.position.y - mDragOffset.y, bounds().getWidth(), bounds().getHeight()));
    layout();
    setNeedsLayout();
    return true;
  }

  bool onPointerUp(const PointerEvent& event) override {
    (void)event;
    if (!mDragging) {
      return false;
    }
    mDragging = false;
    return true;
  }

 protected:
  void applyTheme(const Theme& theme) override {
    mBackground = theme.surfaceRaised;
    mHeader = theme.surface;
    mBorder = theme.controls.border;
    mForeground = theme.controls.foreground;
    mRadius = theme.radius.md;
    setNeedsPaint();
  }

 private:
  std::string mTitle;
  std::shared_ptr<Layer> mContent;
  std::shared_ptr<const graphics::Font> mFont;
  graphics::Color mBackground = graphics::Color::MakeRGBA8(45, 49, 59, 255);
  graphics::Color mHeader = graphics::Color::MakeRGBA8(35, 38, 46, 255);
  graphics::Color mBorder = graphics::Color::MakeRGBA8(72, 80, 94, 255);
  graphics::Color mForeground = graphics::Color::MakeRGBA8(226, 232, 240, 255);
  float mHeaderHeight = 24.0f;
  float mRadius = 4.0f;
  Point mDragOffset{};
  bool mDragging = false;
};

DockView::DockView() = default;
DockView::~DockView() = default;

DockView::DockGroup& DockView::group(const Region region) {
  return mGroups[static_cast<size_t>(region)];
}

const DockView::DockGroup& DockView::group(const Region region) const {
  return mGroups[static_cast<size_t>(region)];
}

void DockView::setFont(std::shared_ptr<const graphics::Font> font) {
  mFont = std::move(font);
  for (FloatingEntry& entry: mFloating) {
    if (entry.layer != nullptr) {
      entry.layer->setFont(mFont);
    }
  }
  setNeedsLayout();
  setNeedsPaint();
}

void DockView::setTabHeaderHeight(const float height) {
  mTabHeaderHeight = std::max(0.0f, height);
  setNeedsLayout();
}

void DockView::setPanel(const Region region, std::shared_ptr<Layer> panel, const float size) {
  clearPanel(region);
  if (panel != nullptr) {
    addPanel(region, {}, std::move(panel), true);
  }
  if (size > 0.0f && region != Region::Center) {
    setSize(region, size);
  }
}

void DockView::clearPanel(const Region region) {
  DockGroup& target = group(region);
  for (const DockItem& item: target.items) {
    if (item.content != nullptr) {
      removeChild(item.content);
    }
  }
  target.items.clear();
  target.active = -1;
  setNeedsLayout();
  setNeedsPaint();
}

const std::shared_ptr<Layer>& DockView::panel(const Region region) const {
  static const std::shared_ptr<Layer> empty;
  const DockGroup& target = group(region);
  return target.active >= 0 && target.active < static_cast<int>(target.items.size())
    ? target.items[static_cast<size_t>(target.active)].content
    : empty;
}

int DockView::addPanel(const Region region, std::string label, std::shared_ptr<Layer> panel,
    const bool activate) {
  if (panel == nullptr) {
    return -1;
  }
  DockGroup& target = group(region);
  const int index = static_cast<int>(target.items.size());
  target.items.push_back(DockItem{ std::move(label), std::move(panel) });
  addChild(target.items.back().content);
  if (target.active < 0) {
    target.active = index;
  }
  if (activate) {
    target.active = index;
  }
  updateVisibility(region);
  setNeedsLayout();
  setNeedsPaint();
  return index;
}

bool DockView::removePanel(const Region region, const int index) {
  DockGroup& target = group(region);
  if (index < 0 || index >= static_cast<int>(target.items.size())) {
    return false;
  }
  if (target.items[static_cast<size_t>(index)].content != nullptr) {
    removeChild(target.items[static_cast<size_t>(index)].content);
  }
  target.items.erase(target.items.begin() + index);
  if (target.items.empty()) {
    target.active = -1;
  } else if (target.active == index) {
    target.active = std::min(index, static_cast<int>(target.items.size()) - 1);
  } else if (target.active > index) {
    --target.active;
  }
  updateVisibility(region);
  setNeedsLayout();
  setNeedsPaint();
  if (mOnPanelChanged) {
    mOnPanelChanged(region, target.active);
  }
  return true;
}

int DockView::panelCount(const Region region) const {
  return static_cast<int>(group(region).items.size());
}

int DockView::activePanel(const Region region) const {
  return group(region).active;
}

void DockView::setActivePanel(const Region region, const int index) {
  DockGroup& target = group(region);
  if (index < 0 || index >= static_cast<int>(target.items.size()) || target.active == index) {
    return;
  }
  target.active = index;
  updateVisibility(region);
  setNeedsLayout();
  setNeedsPaint();
  if (mOnPanelChanged) {
    mOnPanelChanged(region, index);
  }
}

std::shared_ptr<Layer> DockView::panelAt(const Region region, const int index) const {
  const DockGroup& target = group(region);
  if (index < 0 || index >= static_cast<int>(target.items.size())) {
    return nullptr;
  }
  return target.items[static_cast<size_t>(index)].content;
}

void DockView::setSize(const Region region, const float size) {
  const float next = std::max(mMinimumPaneSize, size);
  float* target = nullptr;
  if (region == Region::Left) target = &mLeftSize;
  else if (region == Region::Right) target = &mRightSize;
  else if (region == Region::Bottom) target = &mBottomSize;
  if (target == nullptr || *target == next) {
    return;
  }
  *target = next;
  setNeedsLayout();
  setNeedsPaint();
  if (mOnSizeChanged) {
    mOnSizeChanged(region, next);
  }
}

float DockView::size(const Region region) const {
  switch (region) {
    case Region::Left: return mLeftSize;
    case Region::Right: return mRightSize;
    case Region::Bottom: return mBottomSize;
    case Region::Center: return 0.0f;
  }
  return 0.0f;
}

void DockView::setCollapsed(const Region region, const bool collapsed) {
  bool* flag = nullptr;
  if (region == Region::Left) flag = &mLeftCollapsed;
  else if (region == Region::Right) flag = &mRightCollapsed;
  else if (region == Region::Bottom) flag = &mBottomCollapsed;
  if (flag == nullptr || *flag == collapsed) {
    return;
  }
  *flag = collapsed;
  updateVisibility(region);
  setNeedsLayout();
  setNeedsPaint();
}

bool DockView::collapsed(const Region region) const {
  switch (region) {
    case Region::Left: return mLeftCollapsed;
    case Region::Right: return mRightCollapsed;
    case Region::Bottom: return mBottomCollapsed;
    case Region::Center: return false;
  }
  return false;
}

void DockView::setDividerWidth(const float width) {
  mDividerWidth = std::max(1.0f, width);
  setNeedsLayout();
  setNeedsPaint();
}

void DockView::setMinimumPaneSize(const float size) {
  mMinimumPaneSize = std::max(0.0f, size);
  setNeedsLayout();
}

float DockView::headerHeight(const Region region) const {
  return group(region).items.size() > 1 ? mTabHeaderHeight : 0.0f;
}

Rect DockView::groupBounds(const Region region) const {
  const Rect outer = bounds();
  // RTL swaps the side panels' slots while each keeps its own width: the Left region (usually the
  // navigation panel) moves to the right edge and vice versa.
  const bool rtl = direction() == Direction::Rtl;
  const Region leftSide = rtl ? Region::Right : Region::Left;
  const Region rightSide = rtl ? Region::Left : Region::Right;
  const float bottomHeight = !group(Region::Bottom).items.empty() && !mBottomCollapsed
    ? std::min(mBottomSize, outer.getHeight())
    : 0.0f;
  const float bottomDivider = bottomHeight > 0.0f ? mDividerWidth : 0.0f;
  const float topHeight = std::max(0.0f, outer.getHeight() - bottomHeight - bottomDivider);
  const float leftWidth = !group(leftSide).items.empty() && !collapsed(leftSide)
    ? std::min(size(leftSide), outer.getWidth())
    : 0.0f;
  const float rightWidth = !group(rightSide).items.empty() && !collapsed(rightSide)
    ? std::min(size(rightSide), outer.getWidth())
    : 0.0f;
  const float leftDivider = leftWidth > 0.0f ? mDividerWidth : 0.0f;
  const float rightDivider = rightWidth > 0.0f ? mDividerWidth : 0.0f;
  const float centerLeft = outer.getLeft() + leftWidth + leftDivider;
  const float centerWidth = std::max(0.0f,
    outer.getWidth() - leftWidth - leftDivider - rightWidth - rightDivider);
  const Rect leftSlot{ Rect::MakeXYWH(outer.getLeft(), outer.getTop(), leftWidth, topHeight) };
  const Rect rightSlot{ Rect::MakeXYWH(centerLeft + centerWidth + rightDivider, outer.getTop(),
    rightWidth, topHeight) };
  switch (region) {
    case Region::Left:
      // In RTL the Left region (navigation) occupies the right slot and keeps its own width.
      return rtl ? rightSlot : leftSlot;
    case Region::Center:
      return Rect::MakeXYWH(centerLeft, outer.getTop(), centerWidth, topHeight);
    case Region::Right:
      return rtl ? leftSlot : rightSlot;
    case Region::Bottom:
      return Rect::MakeXYWH(outer.getLeft(), outer.getBottom() - bottomHeight, outer.getWidth(),
        bottomHeight);
  }
  return {};
}

Size DockView::measure(const Size& available) {
  // Without a constraint the dock reports what its content prefers. The centre used to be two
  // hard-coded constants (240 wide, 180 tall), which made an unconstrained dock ignore the size of
  // the page it was hosting.
  Size centre{};
  const DockGroup& center = group(Region::Center);
  if (!center.items.empty() && center.active >= 0 &&
      center.active < static_cast<int>(center.items.size())) {
    if (const std::shared_ptr<Layer>& content = center.items[static_cast<size_t>(center.active)].content) {
      centre = content->measureCached({});
    }
  }
  float dividers = 0.0f;
  if (!group(Region::Left).items.empty()) dividers += mDividerWidth;
  if (!group(Region::Right).items.empty()) dividers += mDividerWidth;
  if (!group(Region::Bottom).items.empty()) dividers += mDividerWidth;
  const float width = available.width > 0.0f ? available.width
    : (group(Region::Left).items.empty() ? 0.0f : mLeftSize) +
      (group(Region::Right).items.empty() ? 0.0f : mRightSize) +
      (center.items.empty() ? 0.0f : centre.width) + dividers;
  const float height = available.height > 0.0f ? available.height
    : (group(Region::Bottom).items.empty() ? 0.0f : mBottomSize) +
      (center.items.empty() && group(Region::Bottom).items.empty() ? 0.0f : centre.height);
  setMeasuredSize({ width, height });
  return measuredSize();
}

void DockView::updateVisibility(const Region region) {
  DockGroup& target = group(region);
  for (size_t index = 0; index < target.items.size(); ++index) {
    if (target.items[index].content != nullptr) {
      target.items[index].content->setVisible(!collapsed(region) &&
        static_cast<int>(index) == target.active);
    }
  }
}

void DockView::layoutGroup(const Region region, const Rect& bounds) {
  DockGroup& target = group(region);
  if (target.items.empty()) {
    return;
  }
  const float header = headerHeight(region);
  const Rect content = Rect::MakeXYWH(bounds.getLeft(), bounds.getTop() + header,
    bounds.getWidth(), std::max(0.0f, bounds.getHeight() - header));
  for (size_t index = 0; index < target.items.size(); ++index) {
    if (target.items[index].content == nullptr) {
      continue;
    }
    // ## A docked panel is sized by the dock, not by its content
    //
    // The group's rectangle comes from the DockView's own bounds and the configured region sizes
    // (`mLeftSize`, `mRightSize`, `mBottomSize`), never from what the panel measures; a change inside
    // the panel therefore cannot move the panel's edges or anything outside it. That is the
    // precondition `hasFixedParentSize()` documents, so a viewport or a large editor inside a dock can
    // be a layout boundary without lying about content-sized children.
    target.items[index].content->noteParentAssignedSize(true);
    target.items[index].content->setBounds(content);
    if (static_cast<int>(index) == target.active) {
      target.items[index].content->layout();
    }
  }
}

void DockView::layout() {
  layoutGroup(Region::Left, groupBounds(Region::Left));
  layoutGroup(Region::Center, groupBounds(Region::Center));
  layoutGroup(Region::Right, groupBounds(Region::Right));
  layoutGroup(Region::Bottom, groupBounds(Region::Bottom));
  for (FloatingEntry& floating: mFloating) {
    if (floating.layer != nullptr) {
      floating.layer->layout();
    }
  }
  clearNeedsLayout();
}

int DockView::tabAt(const Region region, const Point& point) const {
  const DockGroup& target = group(region);
  const Rect bounds = groupBounds(region);
  if (point.y > bounds.getTop() + headerHeight(region)) {
    return -1;
  }
  float left = bounds.getLeft();
  for (size_t index = 0; index < target.items.size(); ++index) {
    const float width = (mFont != nullptr ? mFont->measureWidth(target.items[index].label) : 60.0f) +
      mTabPadding * 2.0f;
    if (point.x >= left && point.x <= left + width) {
      return static_cast<int>(index);
    }
    left += width;
  }
  return -1;
}

void DockView::drawSelf(RenderContext& context) {
  graphics::Paint divider;
  divider.color = mDividerColor;
  divider.style = graphics::Paint::Style::kFill;
  for (const Edge edge: { Edge::Left, Edge::Right, Edge::Bottom }) {
    if ((edge == Edge::Left && (group(Region::Left).items.empty() || mLeftCollapsed)) ||
        (edge == Edge::Right && (group(Region::Right).items.empty() || mRightCollapsed)) ||
        (edge == Edge::Bottom && (group(Region::Bottom).items.empty() || mBottomCollapsed))) {
      continue;
    }
    context.canvas().drawRect(dividerBounds(edge), divider);
  }

  for (const Region region: { Region::Left, Region::Center, Region::Right, Region::Bottom }) {
    const DockGroup& target = group(region);
    if (target.items.size() <= 1) {
      continue;
    }
    const Rect bounds = groupBounds(region);
    graphics::Paint header;
    header.color = mTabBackground;
    header.style = graphics::Paint::Style::kFill;
    context.canvas().drawRect(Rect::MakeXYWH(bounds.getLeft(), bounds.getTop(),
      bounds.getWidth(), mTabHeaderHeight), header);
    float left = bounds.getLeft();
    for (size_t index = 0; index < target.items.size(); ++index) {
      const float width = (mFont != nullptr ? mFont->measureWidth(target.items[index].label) : 60.0f) +
        mTabPadding * 2.0f;
      const Rect tab = Rect::MakeXYWH(left, bounds.getTop(), width, mTabHeaderHeight);
      if (static_cast<int>(index) == target.active) {
        graphics::Paint active;
        active.color = mTabActive;
        active.style = graphics::Paint::Style::kFill;
        context.canvas().drawRect(tab, active);
      }
      if (mFont != nullptr) {
        detail::drawLabel(context, target.items[index].label, tab, *mFont, mTabForeground,
          left + mTabPadding);
      }
      left += width;
    }
  }
}

Rect DockView::dividerBounds(const Edge edge) const {
  const Rect outer = bounds();
  const bool rtl = direction() == Direction::Rtl;
  const Region leftSide = rtl ? Region::Right : Region::Left;
  const Region rightSide = rtl ? Region::Left : Region::Right;
  const float bottomHeight = !group(Region::Bottom).items.empty() && !mBottomCollapsed
    ? std::min(mBottomSize, outer.getHeight())
    : 0.0f;
  const float bottomDivider = bottomHeight > 0.0f ? mDividerWidth : 0.0f;
  const float topHeight = std::max(0.0f, outer.getHeight() - bottomHeight - bottomDivider);
  const float leftWidth = !group(leftSide).items.empty() && !collapsed(leftSide)
    ? std::min(size(leftSide), outer.getWidth())
    : 0.0f;
  const float rightWidth = !group(rightSide).items.empty() && !collapsed(rightSide)
    ? std::min(size(rightSide), outer.getWidth())
    : 0.0f;
  const float rightDivider = rightWidth > 0.0f ? mDividerWidth : 0.0f;
  switch (edge) {
    case Edge::Left:
      return Rect::MakeXYWH(outer.getLeft() + leftWidth, outer.getTop(), mDividerWidth, topHeight);
    case Edge::Right:
      return Rect::MakeXYWH(outer.getRight() - rightWidth - rightDivider, outer.getTop(),
        mDividerWidth, topHeight);
    case Edge::Bottom:
      return Rect::MakeXYWH(outer.getLeft(), outer.getBottom() - bottomHeight - bottomDivider,
        outer.getWidth(), mDividerWidth);
    case Edge::None:
      break;
  }
  return {};
}

DockView::Edge DockView::edgeAt(const Point& point) const {
  for (const Edge edge: { Edge::Bottom, Edge::Left, Edge::Right }) {
    if ((edge == Edge::Left && (group(Region::Left).items.empty() || mLeftCollapsed)) ||
        (edge == Edge::Right && (group(Region::Right).items.empty() || mRightCollapsed)) ||
        (edge == Edge::Bottom && (group(Region::Bottom).items.empty() || mBottomCollapsed))) {
      continue;
    }
    const Rect divider = dividerBounds(edge);
    const Rect expanded = Rect::MakeLTRB(divider.getLeft() - kDragSlop,
      divider.getTop() - kDragSlop, divider.getRight() + kDragSlop,
      divider.getBottom() + kDragSlop);
    if (expanded.contains(point)) {
      return edge;
    }
  }
  return Edge::None;
}

bool DockView::onPointerDown(const PointerEvent& event) {
  for (const Region region: { Region::Left, Region::Center, Region::Right, Region::Bottom }) {
    if (group(region).items.size() <= 1) {
      continue;
    }
    const int index = tabAt(region, event.position);
    if (index >= 0) {
      setActivePanel(region, index);
      return true;
    }
  }

  const Edge edge = edgeAt(event.position);
  if (edge == Edge::None) {
    return false;
  }
  mDragEdge = edge;
  mDragStart = event.position;
  mDragStartSize = edge == Edge::Left ? mLeftSize
    : edge == Edge::Right ? mRightSize
    : mBottomSize;
  return true;
}

bool DockView::onPointerMove(const PointerEvent& event) {
  if (mDragEdge == Edge::None) {
    return false;
  }
  updateDrag(event.position);
  return true;
}

bool DockView::onPointerUp(const PointerEvent& event) {
  (void)event;
  if (mDragEdge == Edge::None) {
    return false;
  }
  mDragEdge = Edge::None;
  return true;
}

void DockView::updateDrag(const Point& point) {
  if (mDragEdge == Edge::Left) {
    setSize(Region::Left, mDragStartSize + (point.x - mDragStart.x));
  } else if (mDragEdge == Edge::Right) {
    setSize(Region::Right, mDragStartSize - (point.x - mDragStart.x));
  } else if (mDragEdge == Edge::Bottom) {
    setSize(Region::Bottom, mDragStartSize - (point.y - mDragStart.y));
  }
}

DockView::FloatingId DockView::addFloatingPanel(std::string title,
    std::shared_ptr<Layer> content, const Rect& bounds) {
  if (content == nullptr) {
    return 0;
  }
  auto layer = std::make_shared<FloatingPanelLayer>(std::move(title), std::move(content), mFont,
    mTabHeaderHeight);
  layer->setBounds(bounds);
  layer->layout();
  layer->setZIndex(1000 + static_cast<int>(mFloating.size()));
  const FloatingId id = mNextFloatingId++;
  mFloating.push_back(FloatingEntry{ id, layer });
  addChild(layer);
  setNeedsLayout();
  setNeedsPaint();
  return id;
}

void DockView::removeFloatingPanel(const FloatingId id) {
  const auto found = std::find_if(mFloating.begin(), mFloating.end(),
    [id](const FloatingEntry& entry) { return entry.id == id; });
  if (found == mFloating.end()) {
    return;
  }
  removeChild(found->layer);
  mFloating.erase(found);
  setNeedsPaint();
}

void DockView::setFloatingBounds(const FloatingId id, const Rect& bounds) {
  const FloatingEntry* entry = findFloating(id);
  if (entry == nullptr || entry->layer == nullptr) {
    return;
  }
  entry->layer->setBounds(bounds);
  entry->layer->layout();
  setNeedsPaint();
}

Rect DockView::floatingBounds(const FloatingId id) const {
  const FloatingEntry* entry = findFloating(id);
  return entry != nullptr && entry->layer != nullptr ? entry->layer->bounds() : Rect{};
}

std::shared_ptr<Layer> DockView::floatingContent(const FloatingId id) const {
  const FloatingEntry* entry = findFloating(id);
  return entry != nullptr && entry->layer != nullptr ? entry->layer->content() : nullptr;
}

void DockView::raiseFloatingPanel(const FloatingId id) {
  const FloatingEntry* entry = findFloating(id);
  if (entry == nullptr || entry->layer == nullptr) {
    return;
  }
  int maximum = 0;
  for (const FloatingEntry& floating: mFloating) {
    if (floating.layer != nullptr) {
      maximum = std::max(maximum, floating.layer->zIndex());
    }
  }
  entry->layer->setZIndex(maximum + 1);
  setNeedsPaint();
}

const DockView::FloatingEntry* DockView::findFloating(const FloatingId id) const {
  const auto found = std::find_if(mFloating.begin(), mFloating.end(),
    [id](const FloatingEntry& entry) { return entry.id == id; });
  return found != mFloating.end() ? &*found : nullptr;
}

void DockView::applyTheme(const Theme& theme) {
  mDividerColor = theme.controls.border;
  mTabBackground = theme.surface;
  mTabActive = theme.surfaceRaised;
  mTabForeground = theme.controls.foreground;
  setNeedsPaint();
}

} // namespace coral::ui
