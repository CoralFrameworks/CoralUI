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

#include <ui/ColorLayer.h>
#include <ui/DockView.h>
#include <ui/Flex.h>
#include <ui/SplitView.h>
#include <ui/TabBar.h>
#include <ui/Window.h>

#include <graphics/Typeface.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

using namespace coral::ui;

std::shared_ptr<const graphics::Font> testFont() {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, 16.0f) : nullptr;
}

PointerEvent pointerAt(const float x, const float y) {
  PointerEvent event;
  event.position = { x, y };
  event.button = PointerButton::Primary;
  return event;
}

} // namespace

TEST(UiSplitView, LaysOutTwoPanesAndUpdatesTheFractionOnDrag) {
  auto view = std::make_shared<SplitView>(Axis::Row, 0.4f);
  auto first = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  auto second = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  view->setPanes(first, second);
  view->measure({ 300.0f, 100.0f });
  view->setBounds(Rect::MakeWH(300.0f, 100.0f));
  view->layout();

  EXPECT_NEAR(first->bounds().getWidth(), (300.0f - 6.0f) * 0.4f, 0.01f);
  EXPECT_NEAR(second->bounds().getLeft(), first->bounds().getRight() + 6.0f, 0.01f);
  EXPECT_TRUE(view->capturesPointerOnPress());

  const float dividerX = first->bounds().getRight() + 3.0f;
  EXPECT_TRUE(view->onPointerDown(pointerAt(dividerX, 50.0f)));
  view->onPointerMove(pointerAt(150.0f, 50.0f));
  EXPECT_NEAR(view->fraction(), (150.0f - 3.0f) / (300.0f - 6.0f), 0.01f);
  EXPECT_TRUE(view->onPointerUp(pointerAt(150.0f, 50.0f)));
}

TEST(UiDockView, LaysOutFourRegionsAndDragsTheLeftDock) {
  auto dock = std::make_shared<DockView>();
  dock->setMinimumPaneSize(40.0f);
  auto left = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  auto center = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  auto right = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto bottom = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 255, 0, 255));
  dock->setPanel(DockView::Region::Left, left, 100.0f);
  dock->setPanel(DockView::Region::Center, center);
  dock->setPanel(DockView::Region::Right, right, 80.0f);
  dock->setPanel(DockView::Region::Bottom, bottom, 60.0f);
  dock->measure({ 400.0f, 300.0f });
  dock->setBounds(Rect::MakeWH(400.0f, 300.0f));
  dock->layout();

  EXPECT_NEAR(left->bounds().getWidth(), 100.0f, 0.01f);
  EXPECT_NEAR(center->bounds().getWidth(), 208.0f, 0.01f);
  EXPECT_NEAR(right->bounds().getLeft(), 320.0f, 0.01f);
  EXPECT_NEAR(bottom->bounds().getHeight(), 60.0f, 0.01f);
  EXPECT_NEAR(bottom->bounds().getTop(), 240.0f, 0.01f);

  const float dividerX = left->bounds().getRight() + 3.0f;
  EXPECT_TRUE(dock->onPointerDown(pointerAt(dividerX, 50.0f)));
  dock->onPointerMove(pointerAt(dividerX + 20.0f, 50.0f));
  EXPECT_FLOAT_EQ(dock->size(DockView::Region::Left), 120.0f);
  dock->onPointerUp(pointerAt(dividerX + 20.0f, 50.0f));
  dock->layout();
  EXPECT_NEAR(left->bounds().getWidth(), 120.0f, 0.01f);
}

TEST(UiDockView, ADockedPanelIsSizedByTheDockNotByItsContent) {
  // The dock computes each panel's rectangle from its own bounds and the configured region sizes, so
  // the panel is a layout boundary: content growing inside it cannot move its edges or the dividers.
  auto dock = std::make_shared<DockView>();
  auto left = std::make_shared<Flex>(Axis::Column);
  auto center = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  auto content = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  left->addChild(content);
  dock->setPanel(DockView::Region::Left, left, 100.0f);
  dock->setPanel(DockView::Region::Center, center);
  dock->measure({ 400.0f, 300.0f });
  dock->setBounds(Rect::MakeWH(400.0f, 300.0f));
  dock->layout();

  EXPECT_TRUE(left->parentAssignedSize())
    << "the dock's region size decides the panel's width, not the panel's measurement";
  EXPECT_TRUE(left->hasFixedParentSize());
  EXPECT_NEAR(left->bounds().getWidth(), 100.0f, 0.01f);

  // A change inside the panel does not reach the dock's layout: the panel's rectangle is the dock's.
  left->setNeedsLayout();
  EXPECT_FALSE(dock->needsLayout()) << "the panel is a boundary, so its layout dirt stops there";
  dock->layout();
  EXPECT_NEAR(left->bounds().getWidth(), 100.0f, 0.01f);
  EXPECT_NEAR(left->bounds().getLeft(), 0.0f, 0.01f);
}

TEST(UiDockView, CollapsingADockGivesItsSpaceToTheCentre) {
  auto dock = std::make_shared<DockView>();
  auto left = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  auto center = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  dock->setPanel(DockView::Region::Left, left, 100.0f);
  dock->setPanel(DockView::Region::Center, center);
  dock->setBounds(Rect::MakeWH(400.0f, 300.0f));
  dock->layout();
  EXPECT_NEAR(center->bounds().getWidth(), 294.0f, 0.01f);
  dock->setCollapsed(DockView::Region::Left, true);
  dock->layout();
  EXPECT_FALSE(left->visible());
  EXPECT_NEAR(center->bounds().getWidth(), 400.0f, 0.01f);
}

TEST(UiDockView, TabsSwitchPanelsInsideARegion) {
  auto dock = std::make_shared<DockView>();
  auto first = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  auto second = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  dock->setBounds(Rect::MakeWH(400.0f, 300.0f));
  dock->addPanel(DockView::Region::Right, "One", first);
  dock->addPanel(DockView::Region::Right, "Two", second);
  dock->layout();
  EXPECT_EQ(dock->panelCount(DockView::Region::Right), 2);
  EXPECT_EQ(dock->activePanel(DockView::Region::Right), 1);
  EXPECT_FALSE(first->visible());
  EXPECT_TRUE(second->visible());
  EXPECT_NEAR(second->bounds().getTop(), second->parent()->bounds().getTop() + 24.0f, 0.01f);

  dock->setActivePanel(DockView::Region::Right, 0);
  EXPECT_TRUE(first->visible());
  EXPECT_FALSE(second->visible());
  const Rect region = dock->panel(DockView::Region::Right)->bounds();
  EXPECT_TRUE(dock->onPointerDown(pointerAt(region.getLeft() + 90.0f, 5.0f)));
  EXPECT_EQ(dock->activePanel(DockView::Region::Right), 1);
  EXPECT_EQ(dock->panelCount(DockView::Region::Right), 2);
}

TEST(UiDockView, AFloatingPanelCanBeDraggedByItsHeader) {
  Window::Options options;
  options.headless = true;
  options.width = 400;
  options.height = 300;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto dock = std::make_shared<DockView>();
  auto content = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 255, 255));
  window->setRoot(dock);
  const auto id = dock->addFloatingPanel("Floating", content,
    Rect::MakeXYWH(40.0f, 40.0f, 140.0f, 90.0f));
  EXPECT_NE(id, 0u);
  EXPECT_EQ(dock->floatingContent(id), content);
  EXPECT_FALSE(dock->floatingBounds(id).isEmpty());

  window->pointerDown(pointerAt(60.0f, 50.0f));
  window->pointerMove(pointerAt(100.0f, 80.0f));
  window->pointerUp(pointerAt(100.0f, 80.0f));
  EXPECT_NEAR(dock->floatingBounds(id).getLeft(), 80.0f, 0.01f);
  EXPECT_NEAR(dock->floatingBounds(id).getTop(), 70.0f, 0.01f);
  dock->removeFloatingPanel(id);
  EXPECT_EQ(dock->floatingContent(id), nullptr);
}

TEST(UiSplitView, WindowKeepsDeliveringTheDragOutsideTheDivider) {
  Window::Options options;
  options.headless = true;
  options.width = 300;
  options.height = 100;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto view = std::make_shared<SplitView>(Axis::Row, 0.4f);
  auto first = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  auto second = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  view->setPanes(first, second);
  window->setRoot(view);

  const float dividerX = first->bounds().getRight() + 3.0f;
  window->pointerDown(pointerAt(dividerX, 50.0f));
  window->pointerMove(pointerAt(20.0f, 50.0f));
  EXPECT_NEAR(view->fraction(), (20.0f - 3.0f) / (300.0f - 6.0f), 0.01f);
  window->pointerUp(pointerAt(20.0f, 50.0f));
}

TEST(UiTabBar, SwitchesVisiblePagesByClickAndKeyboard) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  auto tabs = std::make_shared<TabBar>(font);
  auto first = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  auto second = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  tabs->addTab("One", first);
  tabs->addTab("Two", second);
  tabs->setBounds(Rect::MakeWH(300.0f, 200.0f));
  tabs->layout();
  EXPECT_EQ(tabs->activeIndex(), 0);
  EXPECT_TRUE(first->visible());
  EXPECT_FALSE(second->visible());

  EXPECT_TRUE(tabs->onPointerDown(pointerAt(80.0f, 15.0f)));
  EXPECT_EQ(tabs->activeIndex(), 1);
  EXPECT_FALSE(first->visible());
  EXPECT_TRUE(second->visible());

  tabs->onKeyDown(KeyEvent{ Key::Left, false, false, false, false, false, 0 });
  EXPECT_EQ(tabs->activeIndex(), 0);

  EXPECT_EQ(tabs->content(1), second);
}
