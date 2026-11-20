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
#include <ui/Button.h>
#include <ui/ColorLayer.h>
#include <ui/Element.h>
#include <ui/Reconciler.h>
#include <ui/Motion.h>
#include <ui/RecyclingListView.h>
#include <ui/TextLayer.h>
#include <ui/Window.h>

#include <graphics/Typeface.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace coral::ui;

std::shared_ptr<const graphics::Font> testFont() {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, 14.0f) : nullptr;
}

std::vector<std::string> keys(const size_t count) {
  std::vector<std::string> result;
  result.reserve(count);
  for (size_t i = 0; i < count; ++i) result.push_back("row-" + std::to_string(i));
  return result;
}

ElementPtr row(const std::string& key, const size_t index) {
  return std::make_shared<ColorElement>(index % 2 == 0 ? Color::MakeRGBA8(20, 20, 20, 255)
                                                       : Color::MakeRGBA8(30, 30, 30, 255),
    key);
}

} // namespace anonymous

TEST(UiRecyclingListView, MountsOnlyVisibleRowsAndReusesKeyedLayers) {
  RecyclingListView list([](const std::string& key, const size_t index,
      const RecyclingRowState&) { return row(key, index); }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 100.0f));
  list.setRows(keys(100));
  list.layout();

  const auto first = list.rowLayer("row-0");
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->parent() != nullptr, true);
  EXPECT_LE(list.mountedRows(), 6u);
  EXPECT_GE(list.mountedRows(), 4u);

  list.setScrollOffset(0.0f, 200.0f);
  list.layout();
  EXPECT_EQ(first->parent(), nullptr) << "the row scrolled out and was detached";
  EXPECT_GT(list.recycledRows(), 0u);
  EXPECT_LE(list.mountedRows(), 6u);
  EXPECT_NE(list.rowLayer("row-10"), nullptr);

  list.setScrollOffset(0.0f, 0.0f);
  list.layout();
  EXPECT_EQ(list.rowLayer("row-0"), first) << "the detached row is reused, not rebuilt";
  EXPECT_EQ(first->parent() != nullptr, true);
}

TEST(UiRecyclingListView, RestoresSelectionFocusAndCapsThePool) {
  RecyclingListView list([](const std::string& key, const size_t index,
      const RecyclingRowState& state) {
    return std::make_shared<ColorElement>(state.selected ? Color::MakeRGBA8(40, 90, 160, 255)
                                                         : Color::MakeRGBA8(30, 30, 30, 255),
      key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 60.0f));
  list.setPoolLimit(1);
  list.setRows(keys(100));
  list.layout();
  Layer* focusedLayer = nullptr;
  list.setFocusRequester([&](Layer* layer) { focusedLayer = layer; });
  PointerEvent click;
  click.position = { 40.0f, 25.0f };
  click.button = PointerButton::Primary;
  EXPECT_TRUE(list.onPointerDown(click));
  EXPECT_EQ(list.selectedKey(), "row-1");
  EXPECT_EQ(list.focusedKey(), "row-1");
  const auto selected = list.rowLayer("row-1");
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(std::static_pointer_cast<ColorLayer>(selected)->color(),
    Color::MakeRGBA8(40, 90, 160, 255));
  EXPECT_EQ(focusedLayer, selected.get());

  list.setScrollOffset(0.0f, 400.0f);
  list.layout();
  EXPECT_LE(list.recycledRows(), 1u) << "the pool limit is enforced";
  EXPECT_EQ(list.scrollY(), 400.0f) << "the scroll offset survives model/selection changes";

  list.setScrollOffset(0.0f, 0.0f);
  list.layout();
  const auto restored = list.rowLayer("row-1");
  ASSERT_NE(restored, nullptr);
  EXPECT_TRUE(restored->parent() != nullptr);
  EXPECT_EQ(std::static_pointer_cast<ColorLayer>(restored)->color(),
    Color::MakeRGBA8(40, 90, 160, 255)) << "selection is re-applied by key after recycling";
}

TEST(UiRecyclingListView, WindowFocusIsClearedAndRestoredWithTheRow) {
  Window::Options windowOptions;
  windowOptions.headless = true;
  windowOptions.width = 240;
  windowOptions.height = 160;
  auto window = Window::create(windowOptions);
  if (window == nullptr) GTEST_SKIP() << "no headless window";
  RecyclingListView list([](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<ButtonElement>(key, nullptr, [] {}, key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 60.0f));
  list.setRows(keys(100));
  window->setRoot(std::shared_ptr<Layer>(&list, [](Layer*) {}));
  list.setFocusOwner(window.get());
  list.layout();

  PointerEvent click;
  click.position = { 40.0f, 25.0f };
  click.button = PointerButton::Primary;
  ASSERT_TRUE(list.onPointerDown(click));
  const auto focusedRow = list.rowLayer("row-1");
  ASSERT_NE(focusedRow, nullptr);
  EXPECT_EQ(window->focusedLayer(), focusedRow.get());

  list.setScrollOffset(0.0f, 400.0f);
  list.layout();
  EXPECT_TRUE(list.focusDetached());
  EXPECT_EQ(window->focusedLayer(), nullptr) << "the detached row is not left dangling in Window";

  list.setScrollOffset(0.0f, 0.0f);
  list.layout();
  EXPECT_FALSE(list.focusDetached());
  EXPECT_EQ(window->focusedLayer(), list.rowLayer("row-1").get());
  window->focusLayer(nullptr);
}

TEST(UiRecyclingListView, PoolEvictsByDistanceThenLeastRecentAccess) {
  RecyclingListView list([](const std::string& key, const size_t index,
      const RecyclingRowState&) { return row(key, index); }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 60.0f));
  list.setPoolLimit(2);
  list.setRows(keys(100));
  list.layout();

  const auto near = list.rowLayer("row-2");
  const auto far = list.rowLayer("row-0");
  ASSERT_NE(near, nullptr);
  ASSERT_NE(far, nullptr);

  list.setScrollOffset(0.0f, 400.0f);
  list.layout();
  EXPECT_GT(list.poolEvictions(), 0u);
  EXPECT_LE(list.recycledRows(), 2u);
  EXPECT_EQ(list.rowLayer("row-0"), nullptr) << "the farthest row is evicted first";
  EXPECT_NE(list.rowLayer("row-2"), nullptr) << "the row closest to the viewport stays pooled";

  list.setScrollOffset(0.0f, 0.0f);
  list.layout();
  EXPECT_GT(list.poolHits(), 0u);
  EXPECT_EQ(list.rowLayer("row-2"), near) << "the retained row is reused with its layer identity";
}

TEST(UiRecyclingListView, PoolPolicyCanPreferLruAndProtectNearRows) {
  auto makeList = [](const double distanceWeight, const size_t above, const size_t below) {
    auto list = std::make_unique<RecyclingListView>([](const std::string& key, const size_t index,
        const RecyclingRowState&) { return row(key, index); }, 20.0f);
    list->setBounds(Rect::MakeWH(200.0f, 59.0f));
    RecyclingListView::PoolPolicy policy;
    policy.limit = std::max<size_t>(1, above + below);
    policy.minRetainAbove = above;
    policy.minRetainBelow = below;
    policy.distanceWeight = distanceWeight;
    list->setPoolPolicy(policy);
    list->setRows(keys(100));
    list->layout();
    list->setScrollOffset(0.0f, 400.0f);
    list->layout();
    list->setScrollOffset(0.0f, 0.0f);
    list->layout();
    return list;
  };

  {
    auto distance = makeList(1.0, 0, 0);
    EXPECT_NE(distance->rowLayer("row-20"), nullptr) << "distance keeps the row nearest the viewport";
    EXPECT_EQ(distance->rowLayer("row-22"), nullptr);
  }
  {
    auto lru = makeList(0.0, 0, 0);
    EXPECT_EQ(lru->rowLayer("row-20"), nullptr) << "pure LRU keeps the most recently used row";
    EXPECT_NE(lru->rowLayer("row-22"), nullptr);
  }
  {
    auto below = makeList(0.0, 0, 2);
    EXPECT_NE(below->rowLayer("row-20"), nullptr);
    EXPECT_NE(below->rowLayer("row-21"), nullptr);
    EXPECT_EQ(below->rowLayer("row-22"), nullptr)
      << "minRetainBelow protects the nearest rows below the viewport";
  }
  {
    auto aboveOnly = makeList(0.0, 1, 0);
    EXPECT_EQ(aboveOnly->rowLayer("row-20"), nullptr)
      << "minRetainAbove does not protect rows below the viewport";
    EXPECT_NE(aboveOnly->rowLayer("row-22"), nullptr) << "pure LRU keeps the newest below row";
  }
  {
    auto belowOnly = makeList(0.0, 0, 1);
    EXPECT_NE(belowOnly->rowLayer("row-20"), nullptr)
      << "minRetainBelow protects the nearest below row even under pure LRU";
    EXPECT_EQ(belowOnly->rowLayer("row-22"), nullptr);
  }
}

TEST(UiRecyclingListView, PoolRetentionAcceptsPixelBudgets) {
  RecyclingListView list([](const std::string& key, const size_t index,
      const RecyclingRowState&) { return row(key, index); }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 59.0f));
  RecyclingListView::PoolPolicy policy;
  policy.limit = 2;
  policy.minRetainBelowPixels = 25.0f; // 20px rows, so this means 2 rows
  policy.distanceWeight = 0.0;
  list.setPoolPolicy(policy);
  list.setRows(keys(100));
  list.layout();
  list.setScrollOffset(0.0f, 400.0f);
  list.layout();
  list.setScrollOffset(0.0f, 0.0f);
  list.layout();

  EXPECT_NE(list.rowLayer("row-20"), nullptr);
  EXPECT_NE(list.rowLayer("row-21"), nullptr);
  EXPECT_EQ(list.rowLayer("row-22"), nullptr)
    << "a 25px below-budget protects two 20px rows under pure LRU";
}

TEST(UiRecyclingListView, VariableRowHeightsDriveLayoutAndScrolling) {
  RecyclingListView list([](const std::string& key, const size_t index,
      const RecyclingRowState&) { return row(key, index); }, 20.0f);
  list.setRowHeightProvider([](const std::string& key, const size_t) {
    return key == "row-0" ? 100.0f : 20.0f;
  });
  list.setBounds(Rect::MakeWH(200.0f, 120.0f));
  list.setRows({ "row-0", "row-1", "row-2", "row-3", "row-4",
    "row-5", "row-6", "row-7", "row-8", "row-9" });
  list.layout();

  EXPECT_FLOAT_EQ(list.contentHeight(), 280.0f);
  const auto first = list.rowLayer("row-0");
  ASSERT_NE(first, nullptr);
  EXPECT_FLOAT_EQ(first->bounds().getHeight(), 100.0f);
  EXPECT_FLOAT_EQ(list.rowLayer("row-1")->bounds().getTop(), 100.0f);

  list.setScrollOffset(0.0f, 100.0f);
  list.layout();
  EXPECT_EQ(first->parent(), nullptr) << "the tall first row scrolled out";
  EXPECT_NE(list.rowLayer("row-1"), nullptr);
  EXPECT_NE(list.rowLayer("row-2"), nullptr);
  EXPECT_FLOAT_EQ(list.rowLayer("row-1")->bounds().getTop(), 0.0f);
  EXPECT_FLOAT_EQ(list.rowLayer("row-2")->bounds().getTop(), 20.0f);
}

TEST(UiRecyclingListView, AutoRowHeightMeasuresTheBuiltLayer) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>("Row " + key, font, Color::White(), key);
  }, 5.0f);
  list.setBounds(Rect::MakeWH(240.0f, 120.0f));
  list.setAutoRowHeight(true);
  list.setRows({ "row-0", "row-1", "row-2" });
  list.layout();

  const auto layer = list.rowLayer("row-0");
  ASSERT_NE(layer, nullptr);
  EXPECT_GT(layer->bounds().getHeight(), 5.0f)
    << "the measured text row replaces the initial estimate";
  EXPECT_GT(list.contentHeight(), 15.0f);
}

TEST(UiRecyclingListView, FocusScopePreventsARestoredRowFromStealingFocus) {
  Window::Options options;
  options.headless = true;
  options.width = 240;
  options.height = 160;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no headless window";

  auto root = std::make_shared<Layer>();
  auto list = std::make_shared<RecyclingListView>([](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<ButtonElement>(key, nullptr, [] {}, key);
  }, 20.0f);
  list->setBounds(Rect::MakeWH(200.0f, 60.0f));
  list->setRows(keys(100));
  root->addChild(list);
  window->setRoot(root);
  list->setFocusOwner(window.get());
  list->layout();

  PointerEvent click;
  click.position = { 40.0f, 25.0f };
  click.button = PointerButton::Primary;
  ASSERT_TRUE(list->onPointerDown(click));
  ASSERT_EQ(window->focusedLayer(), list->rowLayer("row-1").get());

  auto modal = std::make_shared<Layer>();
  modal->setFocusScope(true);
  modal->setBounds(Rect::MakeWH(200.0f, 120.0f));
  root->addChild(modal);
  ASSERT_EQ(window->activeFocusScope(), modal.get());

  list->setScrollOffset(0.0f, 400.0f);
  list->layout();
  list->setScrollOffset(0.0f, 0.0f);
  list->layout();
  EXPECT_EQ(window->focusedLayer(), nullptr)
    << "a restored row must not escape an active modal focus scope";
  window->focusLayer(nullptr);
}

TEST(UiRecyclingListView, AutoRowHeightPrefetchesJustOutsideTheViewport) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>("Row " + key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(240.0f, 60.0f));
  list.setAutoRowHeight(true);
  list.setRows(keys(20));
  list.layout();

  EXPECT_NE(list.rowLayer("row-4"), nullptr)
    << "one row below the visible range is built and measured ahead of scrolling";
  EXPECT_EQ(list.rowLayer("row-6"), nullptr) << "the prefetch window stays bounded";
  EXPECT_GT(list.contentHeight(), 20.0f);
}

TEST(UiRecyclingListView, EquivalentNewElementsAreNotMeasuredAgain) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>("stable " + key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(240.0f, 60.0f));
  list.setAutoRowHeight(true);
  list.setRows({ "row-0", "row-1" });
  list.layout();
  const uint64_t firstCount = list.measureCount();
  ASSERT_GT(firstCount, 0u);
  list.layout();
  EXPECT_EQ(list.measureCount(), firstCount)
    << "a structurally equivalent Element does not trigger a second measure";
}

TEST(UiRecyclingListView, DirectLayerFontChangeInvalidatesMeasurement) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>("Row " + key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(240.0f, 60.0f));
  list.setAutoRowHeight(true);
  list.setRows({ "row-0", "row-1" });
  list.layout();
  const uint64_t before = list.measureCount();
  auto layer = std::static_pointer_cast<TextLayer>(list.rowLayer("row-0"));
  ASSERT_NE(layer, nullptr);
  const auto typeface = graphics::Typeface::fromFile(
    std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf");
  ASSERT_NE(typeface, nullptr);
  layer->setFont(graphics::Font::create(typeface, 24.0f));
  list.layout();
  EXPECT_GT(list.measureCount(), before)
    << "a direct TextLayer font change bumps the row font revision";
}

TEST(UiRecyclingListView, FastScrollBoostsThePrefetchWindow) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>("Row " + key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(240.0f, 60.0f));
  list.setAutoRowHeight(true);
  list.setMeasurePrefetchRows(0);
  list.setMaxMeasurePrefetchExtra(4);
  list.setRows(keys(100));
  list.layout();
  EXPECT_EQ(list.effectiveMeasurePrefetchRows(), 0u);

  ScrollEvent scroll;
  scroll.position = { 20.0f, 20.0f };
  scroll.deltaY = -100.0f;
  ASSERT_TRUE(list.onScroll(scroll));
  EXPECT_EQ(list.effectiveMeasurePrefetchRows(), 4u)
    << "100px at a 20px row size requests the maximum extra rows";
}

TEST(UiRecyclingListView, PrefetchBoostExpiresWithTheVelocityWindow) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>("Row " + key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(240.0f, 60.0f));
  list.setAutoRowHeight(true);
  list.setMeasurePrefetchRows(0);
  list.setMaxMeasurePrefetchExtra(4);
  list.setRows(keys(100));
  list.layout();

  ScrollEvent scroll;
  scroll.position = { 20.0f, 20.0f };
  scroll.deltaY = -100.0f;
  ASSERT_TRUE(list.onScroll(scroll));
  EXPECT_EQ(list.effectiveMeasurePrefetchRows(), 4u);
  std::this_thread::sleep_for(std::chrono::milliseconds(160));
  list.layout();
  EXPECT_EQ(list.effectiveMeasurePrefetchRows(), 0u)
    << "the boost decays after the 120ms velocity window";
}

TEST(UiRecyclingListView, UnchangedElementsAreNotMeasuredAgain) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  auto element = std::make_shared<TextElement>("stable", font, Color::White(), "row-0");
  RecyclingListView list([&element](const std::string&, const size_t,
      const RecyclingRowState&) {
    return ElementPtr(element);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(240.0f, 60.0f));
  list.setAutoRowHeight(true);
  list.setRows({ "row-0", "row-1" });
  list.layout();
  const uint64_t firstCount = list.measureCount();
  ASSERT_GT(firstCount, 0u);
  list.layout();
  EXPECT_EQ(list.measureCount(), firstCount)
    << "the same ElementPtr does not trigger a second measure";

  element = std::make_shared<TextElement>("changed", font, Color::White(), "row-0");
  list.layout();
  EXPECT_GT(list.measureCount(), firstCount)
    << "a new ElementPtr invalidates the row measurement";
}

TEST(UiRecyclingListView, NestedFocusScopesUseTheInnermostScope) {
  Window::Options options;
  options.headless = true;
  options.width = 240;
  options.height = 160;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no headless window";

  auto root = std::make_shared<Layer>();
  auto outer = std::make_shared<Layer>();
  outer->setFocusScope(true);
  outer->setBounds(Rect::MakeWH(220.0f, 140.0f));
  auto inner = std::make_shared<Layer>();
  inner->setFocusScope(true);
  inner->setBounds(Rect::MakeWH(120.0f, 60.0f));
  auto list = std::make_shared<RecyclingListView>([](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<ButtonElement>(key, nullptr, [] {}, key);
  }, 20.0f);
  list->setBounds(Rect::MakeWH(200.0f, 60.0f));
  list->setRows(keys(100));
  outer->addChild(list);
  outer->addChild(inner);
  root->addChild(outer);
  window->setRoot(root);
  list->setFocusOwner(window.get());
  list->layout();

  EXPECT_EQ(window->activeFocusScope(), inner.get())
    << "the nested scope is more specific than its parent";

  PointerEvent click;
  click.position = { 40.0f, 25.0f };
  click.button = PointerButton::Primary;
  ASSERT_TRUE(list->onPointerDown(click));
  EXPECT_EQ(window->focusedLayer(), nullptr)
    << "the row lies outside the innermost scope and must not take focus";
  window->focusLayer(nullptr);
}

TEST(UiRecyclingListView, FocusScopeFadeAndDisableStopTrapping) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no headless window";

  auto root = std::make_shared<Layer>();
  auto modal = std::make_shared<Layer>();
  modal->setFocusScope(true);
  modal->setBounds(Rect::MakeWH(120.0f, 80.0f));
  root->addChild(modal);
  window->setRoot(root);
  EXPECT_EQ(window->activeFocusScope(), modal.get());

  modal->setOpacity(0.0f);
  EXPECT_EQ(window->activeFocusScope(), nullptr)
    << "a fully faded scope stops trapping focus before it is destroyed";
  modal->setOpacity(1.0f);
  modal->setFocusScope(false);
  EXPECT_EQ(window->activeFocusScope(), nullptr)
    << "a scope explicitly disabled during close stops trapping focus";
}

TEST(UiRecyclingListView, FocusScopePriorityOverridesPaintOrder) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no headless window";

  auto root = std::make_shared<Layer>();
  auto high = std::make_shared<Layer>();
  high->setFocusScope(true);
  high->setFocusScopePriority(5);
  high->setBounds(Rect::MakeWH(80.0f, 40.0f));
  auto low = std::make_shared<Layer>();
  low->setFocusScope(true);
  low->setBounds(Rect::MakeWH(80.0f, 40.0f));
  root->addChild(high);
  root->addChild(low); // low is later in paint order, but high has explicit priority
  window->setRoot(root);
  EXPECT_EQ(window->activeFocusScope(), high.get());

  low->setFocusScopePriority(10);
  EXPECT_EQ(window->activeFocusScope(), low.get());
}

TEST(UiRecyclingListView, AccessibilityTreeFollowsTabOrder) {
  auto root = std::make_shared<Layer>();
  auto first = std::make_shared<Button>("first", nullptr);
  first->setTabOrder(6);
  auto second = std::make_shared<Button>("second", nullptr);
  second->setTabOrder(2);
  root->addChild(first);
  root->addChild(second);

  const AccessibilityNode tree = accessibilityTree(*root);
  ASSERT_EQ(tree.children.size(), 2u);
  EXPECT_EQ(tree.children[0].name, "second");
  EXPECT_EQ(tree.children[1].name, "first");
  EXPECT_EQ(tree.children[0].tabOrder, 2);

  const auto order = accessibilityFocusOrder(tree);
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0]->name, "second");
  EXPECT_EQ(order[1]->name, "first");

  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 120;
  auto window = Window::create(options);
  if (window != nullptr) {
    window->setRoot(root);
    const auto nodes = window->accessibilityFocusNodes();
    ASSERT_EQ(nodes.size(), 2u);
    EXPECT_EQ(nodes[0].name, "second");
    EXPECT_EQ(nodes[1].name, "first");
  }
}

TEST(UiRecyclingListView, FocusScopePriorityOrdersTabWithinTheActiveScope) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 120;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no headless window";

  auto outer = std::make_shared<Layer>();
  outer->setFocusScope(true);
  outer->setFocusScopePriority(10);
  outer->setBounds(Rect::MakeWH(140.0f, 100.0f));
  auto direct = std::make_shared<Button>("direct", nullptr);
  direct->setBounds(Rect::MakeWH(60.0f, 24.0f));
  auto inner = std::make_shared<Layer>();
  inner->setFocusScope(true);
  inner->setFocusScopePriority(1);
  inner->setBounds(Rect::MakeWH(60.0f, 24.0f));
  auto nested = std::make_shared<Button>("nested", nullptr);
  nested->setBounds(Rect::MakeWH(60.0f, 24.0f));
  inner->addChild(nested);
  outer->addChild(direct);
  outer->addChild(inner);
  window->setRoot(outer);

  KeyEvent tab;
  tab.key = Key::Tab;
  ASSERT_TRUE(window->keyDown(tab));
  EXPECT_EQ(window->focusedLayer(), nested.get())
    << "the higher-priority nested scope is visited first";
  ASSERT_TRUE(window->keyDown(tab));
  EXPECT_EQ(window->focusedLayer(), direct.get());

  // Explicit tab order is independent of both tree order and scope priority.
  direct->setTabOrder(1);
  nested->setTabOrder(2);
  window->focusLayer(nullptr);
  ASSERT_TRUE(window->keyDown(tab));
  EXPECT_EQ(window->focusedLayer(), direct.get());
  ASSERT_TRUE(window->keyDown(tab));
  EXPECT_EQ(window->focusedLayer(), nested.get());
  window->focusLayer(nullptr);
}

TEST(UiRecyclingListView, ElementBuilderWrapsTheVirtualizedList) {
  Reconciler reconciler;
  auto build = [] {
    return std::make_shared<StackElement>(Axis::Column, std::vector<ElementPtr>{
      components::RecyclingList(keys(50),
        [](const std::string& key, const size_t index,
          const RecyclingRowState&) { return row(key, index); }, 20.0f, "list"),
    }, "root");
  };
  const auto root = reconciler.mount(build());
  ASSERT_EQ(root->children().size(), 1u);
  const auto list = std::static_pointer_cast<RecyclingListView>(root->children()[0]);
  ASSERT_NE(list, nullptr);
  list->setBounds(Rect::MakeWH(200.0f, 80.0f));
  list->layout();
  EXPECT_GT(list->mountedRows(), 0u);
  EXPECT_NE(list->rowLayer("row-0"), nullptr);
  EXPECT_EQ(list->rowLayer("row-49"), nullptr) << "far rows stay unmounted";
}

TEST(UiRecyclingListView, KeyboardMovesFocusAndKeepsItVisible) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>("Row " + key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(240.0f, 100.0f));
  std::vector<std::string> rows;
  for (int index = 0; index < 30; ++index) rows.push_back("row-" + std::to_string(index));
  list.setRows(rows);
  list.layout();
  EXPECT_TRUE(list.focusable());

  KeyEvent down;
  down.key = Key::Down;
  ASSERT_TRUE(list.onKeyDown(down));
  EXPECT_EQ(list.focusedKey(), "row-0");
  ASSERT_TRUE(list.onKeyDown(down));
  EXPECT_EQ(list.focusedKey(), "row-1");

  KeyEvent end;
  end.key = Key::End;
  ASSERT_TRUE(list.onKeyDown(end));
  EXPECT_EQ(list.focusedKey(), "row-29") << "End focuses the last row";
  EXPECT_GT(list.scrollY(), 0.0f) << "End scrolls the last row into view";

  KeyEvent home;
  home.key = Key::Home;
  ASSERT_TRUE(list.onKeyDown(home));
  EXPECT_EQ(list.focusedKey(), "row-0");
  EXPECT_FLOAT_EQ(list.scrollY(), 0.0f);

  for (int step = 0; step < 10; ++step) list.onKeyDown(down);
  EXPECT_EQ(list.focusedKey(), "row-10");
  EXPECT_GT(list.scrollY(), 0.0f) << "moving past the viewport scrolls the list";

  int activated = 0;
  list.setOnActivate([&activated](const std::string& key) {
    ++activated;
    EXPECT_EQ(key, "row-10");
  });
  KeyEvent enter;
  enter.key = Key::Enter;
  EXPECT_TRUE(list.onKeyDown(enter));
  EXPECT_EQ(activated, 1);
}

TEST(UiRecyclingListView, ScrollToKeyFocusesAndReveals) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 60.0f));
  std::vector<std::string> rows;
  for (int index = 0; index < 20; ++index) rows.push_back("row-" + std::to_string(index));
  list.setRows(rows);
  list.layout();

  list.scrollToKey("row-15");
  EXPECT_EQ(list.focusedKey(), "row-15");
  EXPECT_GT(list.scrollY(), 0.0f);
  list.ensureKeyVisible("missing");
  EXPECT_EQ(list.focusedKey(), "row-15") << "an unknown key is a no-op";
}

TEST(UiRecyclingListView, StickyHeaderTracksTheNearestSection) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 100.0f));
  std::vector<std::string> rows;
  for (int index = 0; index < 30; ++index) rows.push_back("row-" + std::to_string(index));
  list.setRows(rows);
  list.setStickyKeys({ "row-0", "row-10", "row-20" });
  list.layout();

  EXPECT_EQ(list.stickyHeaderKey(), "row-0") << "the first section is pinned from the start";
  EXPECT_FLOAT_EQ(list.stickyHeaderHeight(), 20.0f);

  list.scrollToKey("row-15");
  EXPECT_EQ(list.stickyHeaderKey(), "row-10") << "scrolling into a section updates the pinned header";
  list.scrollToKey("row-25");
  EXPECT_EQ(list.stickyHeaderKey(), "row-20");

  list.setStickyKeys({});
  EXPECT_EQ(list.stickyHeaderKey(), "");
  EXPECT_FLOAT_EQ(list.stickyHeaderHeight(), 0.0f);
}

TEST(UiRecyclingListView, RowsFadeInWhenATransitionIsConfigured) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setRowTransition(std::chrono::milliseconds(120));
  EXPECT_EQ(list.rowTransition(), std::chrono::milliseconds(120));
  list.setBounds(Rect::MakeWH(200.0f, 200.0f));
  list.setRows({ "row-0", "row-1", "row-2" });
  list.layout();

  const auto layer = list.rowLayer("row-0");
  ASSERT_NE(layer, nullptr);
  EXPECT_FLOAT_EQ(layer->opacity(), 0.0f) << "a newly attached row starts transparent";
  EXPECT_TRUE(layer->hasRunningAnimation());
  layer->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(300));
  EXPECT_FLOAT_EQ(layer->opacity(), 1.0f) << "the fade finishes at full opacity";
}

TEST(UiRecyclingListView, RowsFadeOutBeforeTheyAreDetached) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setRowTransition(std::chrono::milliseconds(120));
  list.setBounds(Rect::MakeWH(200.0f, 200.0f));
  list.setRows({ "row-0", "row-1" });
  list.layout();
  const auto leaving = list.rowLayer("row-0");
  ASSERT_NE(leaving, nullptr);
  ASSERT_NE(leaving->parent(), nullptr);

  list.setRows({ "row-1", "row-2" });
  list.layout();
  EXPECT_TRUE(leaving->hasRunningAnimation()) << "the removed row fades instead of vanishing";
  EXPECT_NE(leaving->parent(), nullptr) << "and stays attached while it fades";

  leaving->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(300));
  list.onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(300));
  EXPECT_EQ(leaving->parent(), nullptr) << "the finished row is detached";
}

TEST(UiRecyclingListView, StickyHeaderIsPinnedWhileScrolling) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 100.0f));
  std::vector<std::string> rows;
  for (int index = 0; index < 30; ++index) rows.push_back("row-" + std::to_string(index));
  list.setRows(rows);
  list.setStickyKeys({ "row-0", "row-10", "row-20" });
  list.layout();
  list.onFrame(std::chrono::steady_clock::now());

  const auto pinnedAtTop = [&list]() -> std::shared_ptr<TextLayer> {
    for (const auto& child: list.content()->children()) {
      if (child != nullptr && child->zIndex() == 5000) {
        return std::dynamic_pointer_cast<TextLayer>(child);
      }
    }
    return nullptr;
  };
  auto pinned = pinnedAtTop();
  ASSERT_NE(pinned, nullptr) << "a pinned header layer exists";
  EXPECT_EQ(pinned->text(), "row-0");

  list.scrollToKey("row-15");
  list.onFrame(std::chrono::steady_clock::now());
  pinned = pinnedAtTop();
  ASSERT_NE(pinned, nullptr);
  EXPECT_EQ(pinned->text(), "row-10") << "scrolling into a section repins the header";
  EXPECT_FLOAT_EQ(pinned->bounds().getTop(),
    list.content()->bounds().getTop() + list.scrollY())
    << "the pinned header stays at the viewport top";

  list.setStickyKeys({});
  list.onFrame(std::chrono::steady_clock::now());
  EXPECT_EQ(pinnedAtTop(), nullptr) << "clearing the keys removes the pinned header";
}

TEST(UiRecyclingListView, RowsAnimateWhenAnInsertionShiftsThem) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setRowTransition(std::chrono::milliseconds(120));
  list.setBounds(Rect::MakeWH(200.0f, 200.0f));
  list.setRows({ "row-0", "row-1", "row-2" });
  list.layout();
  const auto moved = list.rowLayer("row-0");
  ASSERT_NE(moved, nullptr);
  EXPECT_FLOAT_EQ(moved->bounds().getTop(), 0.0f);

  list.setRows({ "new", "row-0", "row-1", "row-2" });
  list.layout();
  EXPECT_TRUE(moved->animatingBounds()) << "the shifted row animates to its new slot";
  EXPECT_FLOAT_EQ(moved->bounds().getTop(), 0.0f) << "it starts where it was";

  moved->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(300));
  EXPECT_FLOAT_EQ(moved->bounds().getTop(), 20.0f) << "and lands one row lower";
}

TEST(UiRecyclingListView, RowsAnimateWhenTheirHeightChanges) {
  const auto small = testFont();
  if (small == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([small](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, small, Color::White(), key);
  }, 20.0f);
  list.setRowTransition(std::chrono::milliseconds(120));
  list.setBounds(Rect::MakeWH(200.0f, 200.0f));
  list.setRows({ "row-0" });
  list.layout();
  const auto row = list.rowLayer("row-0");
  ASSERT_NE(row, nullptr);
  const float smallHeight = row->bounds().getHeight();
  ASSERT_GT(smallHeight, 0.0f);

  list.setRowHeight(40.0f);
  list.layout();
  EXPECT_TRUE(row->animatingBounds()) << "a taller row height animates instead of snapping";
  EXPECT_FLOAT_EQ(row->bounds().getHeight(), smallHeight) << "it starts at the old height";
  row->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(300));
  EXPECT_GT(row->bounds().getHeight(), smallHeight) << "and ends taller";
}

TEST(UiRecyclingListView, MoveTransitionCanBeDecoupledFromEnterFade) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setRowTransition(std::chrono::milliseconds(120));
  list.setRowMoveTransition(std::chrono::milliseconds(0));
  EXPECT_EQ(list.rowMoveTransition(), std::chrono::milliseconds(0));
  list.setBounds(Rect::MakeWH(200.0f, 200.0f));
  list.setRows({ "row-0", "row-1" });
  list.layout();
  const auto moved = list.rowLayer("row-0");
  ASSERT_NE(moved, nullptr);

  list.setRows({ "new", "row-0", "row-1" });
  list.layout();
  EXPECT_FALSE(moved->animatingBounds()) << "a 0 move duration snaps instead of animating";
  EXPECT_FLOAT_EQ(moved->bounds().getTop(), 20.0f);
}

TEST(UiRecyclingListView, ReduceMotionDisablesRowTransitions) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setRowTransition(std::chrono::milliseconds(120));
  list.setBounds(Rect::MakeWH(200.0f, 200.0f));

  setReduceMotion(true);
  list.setRows({ "row-0", "row-1" });
  list.layout();
  const auto row = list.rowLayer("row-0");
  ASSERT_NE(row, nullptr);
  EXPECT_FLOAT_EQ(row->opacity(), 1.0f) << "no enter fade under reduce motion";
  EXPECT_FALSE(row->hasRunningAnimation());

  list.setRows({ "row-1" });
  list.layout();
  EXPECT_EQ(row->parent(), nullptr) << "the removed row is detached immediately";
  setReduceMotion(false);
}

TEST(UiRecyclingListView, ScrollAnchoringKeepsTheTopRowInPlace) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 100.0f));
  std::vector<std::string> rows;
  for (int index = 0; index < 30; ++index) rows.push_back("row-" + std::to_string(index));
  list.setRows(rows);
  list.layout();
  list.scrollToKey("row-10");
  list.layout();
  const float before = list.scrollY();
  ASSERT_GT(before, 0.0f);

  list.setRowHeight(40.0f);
  list.layout();
  EXPECT_FLOAT_EQ(list.scrollY(), before * 2.0f)
    << "row-10 stays at the viewport top when every row doubles in height";
  EXPECT_TRUE(list.scrollAnchoring());
  list.setScrollAnchoring(false);
  list.setRowHeight(20.0f);
  list.layout();
  EXPECT_FALSE(list.scrollAnchoring());
}

TEST(UiRecyclingListView, ScrollAnchoringSurvivesTheAnchorRowBeingRemoved) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 100.0f));
  std::vector<std::string> rows;
  for (int index = 0; index < 30; ++index) rows.push_back("row-" + std::to_string(index));
  list.setRows(rows);
  list.layout();
  list.scrollToKey("row-10");
  list.layout();
  const float before = list.scrollY();
  ASSERT_GT(before, 0.0f);

  rows.erase(rows.begin() + 10);
  list.setRows(rows);
  list.layout();
  EXPECT_NEAR(list.scrollY(), before, 0.5f)
    << "the viewport keeps its position when the anchored row disappears";
  EXPECT_NE(list.rowLayer("row-11"), nullptr) << "the following row is now at the top";
}

TEST(UiRecyclingListView, RowsCarryTheirKeyAsAccessibleName) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 200.0f));
  list.setRows({ "row-0", "row-1" });
  list.layout();
  const auto row = list.rowLayer("row-0");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->accessibleName(), "row-0");
  EXPECT_EQ(row->accessibleRole(), AccessibilityRole::ListItem);

  const AccessibilityNode tree = accessibilityTree(list);
  bool found = false;
  const auto search = [&found](const auto& self, const AccessibilityNode& node) -> void {
    if (node.name == "row-0") found = true;
    for (const auto& child: node.children) self(self, child);
  };
  search(search, tree);
  EXPECT_TRUE(found) << "the row appears as a named accessibility node";
}

TEST(UiRecyclingListView, AccessibilitySelectionFollowsTheSelectedKey) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setBounds(Rect::MakeWH(200.0f, 300.0f));
  list.setRows({ "intro", "broll", "outro" });
  list.layout();

  const auto find = [](const AccessibilityNode& root, const std::string& name)
      -> const AccessibilityNode* {
    const auto self = [&](const auto& recurse, const AccessibilityNode& node)
        -> const AccessibilityNode* {
      if (node.name == name) return &node;
      for (const auto& child : node.children) {
        if (const auto* found = recurse(recurse, child)) return found;
      }
      return nullptr;
    };
    return self(self, root);
  };
  const AccessibilityNode tree = accessibilityTree(list);
  const AccessibilityNode* row = find(tree, "broll");
  ASSERT_NE(row, nullptr) << "the row appears in the tree";
  EXPECT_TRUE(hasAccessibilityAction(row->actions, AccessibilityAction::Select));
  EXPECT_FALSE(row->selected);

  // The list, not the row, owns the selection; selecting the node asks the list.
  const std::string path = row->path;
  Layer* layer = accessibilityLayerAtPath(list, path);
  ASSERT_NE(layer, nullptr);
  EXPECT_TRUE(layer->performAccessibilityAction(AccessibilityAction::Select, {}));
  EXPECT_EQ(list.selectedKey(), "broll");
  const AccessibilityNode after = accessibilityTree(list);
  const AccessibilityNode* selected = find(after, "broll");
  ASSERT_NE(selected, nullptr);
  EXPECT_TRUE(selected->selected);
}

TEST(UiRecyclingListView, ScrollAnchoringSuppressesTheMoveAnimation) {
  const auto font = testFont();
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  RecyclingListView list([font](const std::string& key, const size_t,
      const RecyclingRowState&) {
    return std::make_shared<TextElement>(key, font, Color::White(), key);
  }, 20.0f);
  list.setRowTransition(std::chrono::milliseconds(120));
  list.setBounds(Rect::MakeWH(200.0f, 100.0f));
  std::vector<std::string> rows;
  for (int index = 0; index < 30; ++index) rows.push_back("row-" + std::to_string(index));
  list.setRows(rows);
  list.layout();
  list.scrollToKey("row-10");
  list.layout();
  const auto anchor = list.rowLayer("row-10");
  ASSERT_NE(anchor, nullptr);
  const float before = list.scrollY();
  ASSERT_GT(before, 0.0f);

  list.setRowHeight(40.0f);
  list.layout();
  EXPECT_FALSE(anchor->animatingBounds())
    << "the viewport already compensated, so the row snaps instead of sliding twice";
  EXPECT_NEAR(list.scrollY(), before * 2.0f, 0.5f);
}