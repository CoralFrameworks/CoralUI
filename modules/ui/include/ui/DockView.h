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

#ifndef CORAL_UI_DOCKVIEW_H
#define CORAL_UI_DOCKVIEW_H

#include <ui/Types.h>

#include <ui/Layer.h>


#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

struct Theme;

/**
 * A four-region dock layout: left, centre, right and bottom.
 *
 * A region can hold more than one panel; two or more panels show a tab header and only the active
 * panel occupies the content area. The left/right docks keep an explicit width and the bottom dock an
 * explicit height; the centre gets what is left. Every visible edge has a draggable divider.
 *
 * Floating panels are independent children with a higher z-index. They keep their own rect and can
 * be dragged by their header; docking a floating panel back into a region is the caller's policy.
 */
class DockView final : public Layer {
 public:
  enum class Region : uint8_t {
    Left,
    Center,
    Right,
    Bottom,
  };

  using FloatingId = uint64_t;

  DockView();
  ~DockView() override;

  void setFont(std::shared_ptr<const Font> font);
  void setTabHeaderHeight(float height);
  [[nodiscard]] float tabHeaderHeight() const noexcept {
    return mTabHeaderHeight;
  }

  // Single-panel convenience API.
  void setPanel(Region region, std::shared_ptr<Layer> panel, float size = 200.0f);
  void clearPanel(Region region);
  [[nodiscard]] const std::shared_ptr<Layer>& panel(Region region) const;

  // Tabbed region API.
  int addPanel(Region region, std::string label, std::shared_ptr<Layer> panel,
    bool activate = true);
  bool removePanel(Region region, int index);
  [[nodiscard]] int panelCount(Region region) const;
  [[nodiscard]] int activePanel(Region region) const;
  void setActivePanel(Region region, int index);
  [[nodiscard]] std::shared_ptr<Layer> panelAt(Region region, int index) const;
  void setOnPanelChanged(std::function<void(Region, int)> callback) {
    mOnPanelChanged = std::move(callback);
  }

  void setSize(Region region, float size);
  [[nodiscard]] float size(Region region) const;
  void setCollapsed(Region region, bool collapsed);
  [[nodiscard]] bool collapsed(Region region) const;

  void setDividerWidth(float width);
  [[nodiscard]] float dividerWidth() const noexcept {
    return mDividerWidth;
  }
  void setMinimumPaneSize(float size);
  void setOnSizeChanged(std::function<void(Region, float)> callback) {
    mOnSizeChanged = std::move(callback);
  }

  // Floating panels.
  FloatingId addFloatingPanel(std::string title, std::shared_ptr<Layer> panel, const Rect& bounds);
  void removeFloatingPanel(FloatingId id);
  void setFloatingBounds(FloatingId id, const Rect& bounds);
  [[nodiscard]] Rect floatingBounds(FloatingId id) const;
  [[nodiscard]] std::shared_ptr<Layer> floatingContent(FloatingId id) const;
  void raiseFloatingPanel(FloatingId id);

  [[nodiscard]] bool capturesPointerOnPress() const override {
    return true;
  }

  Size measure(const Size& available) override;
  void layout() override;
  /** A dock is a boundary only when its parent pinned its size. */
  [[nodiscard]] bool isLayoutBoundary() const override {
    return hasFixedParentSize();
  }
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  class FloatingPanelLayer;

  struct DockItem {
    std::string label;
    std::shared_ptr<Layer> content;
  };
  struct DockGroup {
    std::vector<DockItem> items;
    int active = -1;
  };
  struct FloatingEntry {
    FloatingId id = 0;
    std::shared_ptr<FloatingPanelLayer> layer;
  };
  enum class Edge : uint8_t {
    None,
    Left,
    Right,
    Bottom,
  };

  [[nodiscard]] DockGroup& group(Region region);
  [[nodiscard]] const DockGroup& group(Region region) const;
  [[nodiscard]] Rect groupBounds(Region region) const;
  [[nodiscard]] float headerHeight(Region region) const;
  [[nodiscard]] int tabAt(Region region, const Point& point) const;
  [[nodiscard]] Rect dividerBounds(Edge edge) const;
  [[nodiscard]] Edge edgeAt(const Point& point) const;
  void updateDrag(const Point& point);
  void updateVisibility(Region region);
  void layoutGroup(Region region, const Rect& bounds);
  [[nodiscard]] const FloatingEntry* findFloating(FloatingId id) const;

  std::array<DockGroup, 4> mGroups;
  std::shared_ptr<const Font> mFont;
  std::function<void(Region, float)> mOnSizeChanged;
  std::function<void(Region, int)> mOnPanelChanged;
  std::vector<FloatingEntry> mFloating;
  FloatingId mNextFloatingId = 1;
  float mLeftSize = 200.0f;
  float mRightSize = 200.0f;
  float mBottomSize = 160.0f;
  float mDividerWidth = 6.0f;
  float mTabHeaderHeight = 24.0f;
  float mTabPadding = 8.0f;
  float mMinimumPaneSize = 80.0f;
  bool mLeftCollapsed = false;
  bool mRightCollapsed = false;
  bool mBottomCollapsed = false;
  Color mDividerColor = Color::MakeRGBA8(62, 67, 78, 255);
  Color mTabBackground = Color::MakeRGBA8(35, 38, 46, 255);
  Color mTabActive = Color::MakeRGBA8(52, 56, 66, 255);
  Color mTabForeground = Color::MakeRGBA8(226, 232, 240, 255);
  Edge mDragEdge = Edge::None;
  Point mDragStart{};
  float mDragStartSize = 0.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_DOCKVIEW_H
