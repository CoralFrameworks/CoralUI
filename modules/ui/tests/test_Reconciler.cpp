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

#include <ui/Button.h>
#include <ui/ColorLayer.h>
#include <ui/ContentHost.h>
#include <ui/ContentView.h>
#include <ui/DockView.h>
#include <ui/CheckBox.h>
#include <ui/ColorButton.h>
#include <ui/Spinner.h>
#include <ui/Element.h>
#include <ui/IconButton.h>
#include <ui/ProgressBar.h>
#include <ui/RadioButton.h>
#include <ui/SegmentedControl.h>
#include <ui/Table.h>
#include <ui/Toggle.h>
#include <ui/ElementRegistry.h>
#include <ui/ElementHost.h>
#include <ui/Flex.h>
#include <ui/Grid.h>
#include <ui/ListView.h>
#include <ui/Reconciler.h>
#include <ui/ScrollView.h>
#include <ui/Slider.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/Window.h>

#include <graphics/Typeface.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using namespace coral::ui;

std::shared_ptr<const graphics::Font> testFont() {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  return typeface != nullptr ? graphics::Font::create(typeface, 14.0f) : nullptr;
}

std::shared_ptr<StackElement> stack(std::vector<ElementPtr> children) {
  return std::make_shared<StackElement>(Axis::Column, std::move(children), "root");
}

} // namespace

TEST(UiReconciler, WhenSwitchesBranchesAndForEachKeepsKeys) {
  Reconciler reconciler;
  bool showLabel = true;
  auto build = [&] {
    return stack({ components::When(showLabel,
      std::make_shared<TextElement>("shown", testFont(), Color::White(), "label"),
      std::make_shared<ColorElement>(Color::MakeRGBA8(4, 4, 4, 255), "fallback"),
      "conditional") });
  };
  const auto root = reconciler.mount(build());
  ASSERT_EQ(root->children().size(), 1u);
  EXPECT_NE(std::dynamic_pointer_cast<TextLayer>(root->children()[0]), nullptr);

  showLabel = false;
  reconciler.update(build());
  ASSERT_EQ(root->children().size(), 1u);
  EXPECT_NE(std::dynamic_pointer_cast<ColorLayer>(root->children()[0]), nullptr);
}

TEST(UiReconciler, ForEachMapsItemsToKeyedChildren) {
  Reconciler reconciler;
  std::vector<std::string> rows = { "a", "b" };
  auto build = [&] {
    return stack(components::ForEach(rows, [](const std::string& row, const size_t index) {
      return std::make_shared<ColorElement>(Color::MakeRGBA8(
        static_cast<uint8_t>(10 + index), 0, 0, 255), "row-" + row);
    }));
  };
  const auto root = reconciler.mount(build());
  ASSERT_EQ(root->children().size(), 2u);
  const auto first = root->children()[0];
  const auto second = root->children()[1];

  rows = { "b", "a" };
  reconciler.update(build());
  ASSERT_EQ(root->children().size(), 2u);
  EXPECT_EQ(root->children()[0], second);
  EXPECT_EQ(root->children()[1], first);
}

TEST(UiReconciler, ComponentsRebuildTheirSubtreeWithoutReplacingTheirLayer) {
  Reconciler reconciler;
  int value = 1;
  auto build = [&] {
    return stack({ components::Component([&] {
      return std::make_shared<TextElement>("value " + std::to_string(value), testFont(),
        Color::White(), "text");
    }, "component") });
  };
  const auto root = reconciler.mount(build());
  const auto label = std::static_pointer_cast<TextLayer>(root->children()[0]);
  EXPECT_EQ(label->text(), "value 1");

  value = 2;
  reconciler.update(build());
  EXPECT_EQ(root->children()[0], label);
  EXPECT_EQ(label->text(), "value 2");
}

TEST(UiReconciler, PropsUpdateTheWrappedLayerInPlace) {
  Reconciler reconciler;
  ElementProps props;
  props.bounds = Rect::MakeXYWH(10.0f, 12.0f, 120.0f, 24.0f);
  props.opacity = 0.5f;
  props.visible = false;
  auto build = [&] {
    return stack({ components::WithProps(
      std::make_shared<TextElement>("wrapped", testFont(), Color::White(), "inner"),
      props, "wrapper") });
  };
  const auto root = reconciler.mount(build());
  const auto layer = root->children()[0];
  EXPECT_EQ(layer->bounds(), Rect::MakeXYWH(10.0f, 12.0f, 120.0f, 24.0f));
  EXPECT_FLOAT_EQ(layer->opacity(), 0.5f);
  EXPECT_FALSE(layer->visible());

  props.bounds = Rect::MakeXYWH(0.0f, 0.0f, 40.0f, 20.0f);
  props.visible = true;
  reconciler.update(build());
  EXPECT_EQ(root->children()[0], layer);
  EXPECT_EQ(layer->bounds(), Rect::MakeXYWH(0.0f, 0.0f, 40.0f, 20.0f));
  EXPECT_TRUE(layer->visible());
}

TEST(UiReconciler, ButtonsAndTextInputsKeepTheirLayerAcrossUpdates) {
  Reconciler reconciler;
  std::string fieldText = "first";
  bool enabled = false;
  auto build = [&] {
    return stack({
      std::make_shared<ButtonElement>("Run", testFont(), [] {}, "button", enabled),
      std::make_shared<TextInputElement>(fieldText, testFont(), [](const std::string&) {}, "input"),
    });
  };
  const auto root = reconciler.mount(build());
  const auto button = std::static_pointer_cast<Button>(root->children()[0]);
  const auto input = std::static_pointer_cast<TextInput>(root->children()[1]);
  ASSERT_NE(button, nullptr);
  ASSERT_NE(input, nullptr);
  EXPECT_EQ(button->label(), "Run");
  EXPECT_FALSE(button->enabled());
  KeyEvent space;
  space.key = Key::Space;
  EXPECT_FALSE(button->onKeyDown(space));
  EXPECT_EQ(button->activationCount(), 0);
  EXPECT_EQ(input->text(), "first");

  fieldText = "second";
  enabled = true;
  reconciler.update(build());
  EXPECT_EQ(root->children()[0], button);
  EXPECT_EQ(root->children()[1], input);
  EXPECT_TRUE(button->enabled());
  EXPECT_TRUE(button->onKeyDown(space));
  EXPECT_EQ(button->activationCount(), 1);
  EXPECT_EQ(input->text(), "second");
}

TEST(UiReconciler, ListViewKeepsItsLayerAndReplacesItsModel) {
  Reconciler reconciler;
  std::vector<std::string> items = { "one", "two" };
  auto build = [&] {
    return stack({ components::ListView(items, testFont(), 24.0f, {}, 0, "list") });
  };
  const auto root = reconciler.mount(build());
  const auto list = std::static_pointer_cast<ListView>(root->children()[0]);
  ASSERT_NE(list, nullptr);
  EXPECT_EQ(list->items(), items);

  items = { "two", "three", "four" };
  reconciler.update(build());
  EXPECT_EQ(root->children()[0], list);
  EXPECT_EQ(list->items(), items);
}

TEST(UiReconciler, RecyclingListElementsCompareTheirBuiltRows) {
  const auto font = testFont();
  auto make = [font](const std::string& text) {
    return std::make_shared<RecyclingListElement>(std::vector<std::string>{ "row-0" },
      [font, text](const std::string& key, const size_t, const RecyclingRowState&) {
        return std::make_shared<TextElement>(text, font, Color::White(), key);
      }, 20.0f, "list");
  };
  const auto first = make("stable");
  const auto equivalent = make("stable");
  EXPECT_TRUE(first->measureEquivalent(*equivalent));
  const auto changed = make("changed");
  EXPECT_FALSE(first->measureEquivalent(*changed));
}

TEST(UiReconciler, RecyclingListSnapshotCanSkipBuildingAllRows) {
  int builds = 0;
  const std::vector<std::string> keys{ "a", "b", "c" };
  auto builder = [&](const std::string& key, const size_t, const RecyclingRowState&) {
    ++builds;
    return std::make_shared<ColorElement>(Color::White(), key);
  };
  const auto snapshot = components::MakeRecyclingListSnapshot(keys, builder);
  const auto changedSnapshot = components::MakeRecyclingListSnapshot(keys,
    [](const std::string&, const size_t index, const RecyclingRowState&) {
      return std::make_shared<ColorElement>(Color::White(), "different-" + std::to_string(index));
    });
  builds = 0;
  auto make = [&](const RecyclingListElement::RowSnapshotPtr& rows) {
    return std::make_shared<RecyclingListElement>(keys, builder, 20.0f, "list", rows);
  };
  const auto first = make(snapshot);
  const auto same = make(snapshot);
  EXPECT_TRUE(first->measureEquivalent(*same));
  EXPECT_EQ(builds, 0) << "a supplied row snapshot is compared without invoking the builder";
  const auto changed = make(changedSnapshot);
  EXPECT_FALSE(first->measureEquivalent(*changed));
  EXPECT_EQ(builds, 0);
}

TEST(UiReconciler, ComponentElementsCacheTheirBuiltSubtree) {
  int value = 1;
  const auto font = testFont();
  auto build = [&] {
    return std::make_shared<ComponentElement>([&] {
      return std::make_shared<TextElement>("value " + std::to_string(value), font,
        Color::White(), "text");
    }, "component");
  };
  const auto first = build();
  const auto equivalent = build();
  EXPECT_TRUE(first->measureEquivalent(*equivalent));
  value = 2;
  const auto changed = build();
  EXPECT_FALSE(first->measureEquivalent(*changed));
}

TEST(UiReconciler, MountsLeavesAndAStack) {
  Reconciler reconciler;
  auto label = std::make_shared<TextElement>("hello", testFont(), Color::White(), "label");
  auto color = std::make_shared<ColorElement>(Color::MakeRGBA8(1, 2, 3, 255), "fill");
  auto root = stack({ label, color });
  const auto layer = reconciler.mount(root);
  ASSERT_NE(layer, nullptr);
  ASSERT_EQ(layer->children().size(), 2u);
  EXPECT_EQ(std::static_pointer_cast<TextLayer>(layer->children()[0])->text(), "hello");
  EXPECT_EQ(std::static_pointer_cast<ColorLayer>(layer->children()[1])->color(),
    Color::MakeRGBA8(1, 2, 3, 255));
  EXPECT_TRUE(reconciler.mounted());
}

TEST(UiReconciler, UpdatesPropsWithoutReplacingTheLayer) {
  Reconciler reconciler;
  auto initial = stack({ std::make_shared<TextElement>("one", testFont(), Color::White(), "label") });
  const auto root = reconciler.mount(initial);
  auto labelLayer = std::static_pointer_cast<TextLayer>(root->children()[0]);

  auto next = stack({ std::make_shared<TextElement>("two", testFont(),
    Color::MakeRGBA8(9, 8, 7, 255), "label") });
  reconciler.update(next);
  EXPECT_EQ(reconciler.root(), root);
  EXPECT_EQ(root->children()[0], labelLayer);
  EXPECT_EQ(labelLayer->text(), "two");
  EXPECT_EQ(labelLayer->color(), Color::MakeRGBA8(9, 8, 7, 255));
}

TEST(UiReconciler, KeyedChildrenKeepIdentityAcrossAReorder) {
  Reconciler reconciler;
  const auto red = std::make_shared<ColorElement>(Color::MakeRGBA8(255, 0, 0, 255), "red");
  const auto green = std::make_shared<ColorElement>(Color::MakeRGBA8(0, 255, 0, 255), "green");
  const auto root = reconciler.mount(stack({ red, green }));
  const auto redLayer = root->children()[0];
  const auto greenLayer = root->children()[1];

  reconciler.update(stack({ green, red }));
  EXPECT_EQ(root->children()[0], greenLayer);
  EXPECT_EQ(root->children()[1], redLayer);
  EXPECT_EQ(std::static_pointer_cast<ColorLayer>(root->children()[0])->color(),
    Color::MakeRGBA8(0, 255, 0, 255));
}

TEST(UiReconciler, AKeyedTypeChangeReplacesOnlyThatChild) {
  Reconciler reconciler;
  const auto root = reconciler.mount(stack({
    std::make_shared<TextElement>("label", testFont(), Color::White(), "slot"),
    std::make_shared<ColorElement>(Color::MakeRGBA8(1, 1, 1, 255), "fill"),
  }));
  const auto fill = root->children()[1];
  reconciler.update(stack({
    std::make_shared<ColorElement>(Color::MakeRGBA8(4, 5, 6, 255), "slot"),
    std::make_shared<ColorElement>(Color::MakeRGBA8(1, 1, 1, 255), "fill"),
  }));
  ASSERT_EQ(root->children().size(), 2u);
  EXPECT_NE(root->children()[0], nullptr);
  EXPECT_EQ(root->children()[1], fill);
  EXPECT_EQ(std::static_pointer_cast<ColorLayer>(root->children()[0])->color(),
    Color::MakeRGBA8(4, 5, 6, 255));
}

TEST(UiReconciler, ElementHostRebuildsWhenABoundPropertyChanges) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr) << Window::lastCreationError();

  coral::observer::Property<std::string> title("first");
  int builds = 0;
  ElementHost host(*window, [&] {
    ++builds;
    return std::make_shared<StackElement>(Axis::Column,
      std::vector<ElementPtr>{
        std::make_shared<TextElement>(title.get(), font, Color::White(), "title") },
      "root");
  });
  host.mount();
  EXPECT_EQ(builds, 1);
  auto binding = host.bind(title);
  window->setRoot(host.reconciler().root());
  ASSERT_EQ(host.reconciler().root()->children().size(), 1u);
  EXPECT_EQ(std::static_pointer_cast<TextLayer>(host.reconciler().root()->children()[0])->text(),
    "first");

  title.set("second");
  title.set("third");
  window->pump();
  EXPECT_EQ(builds, 2) << "two property writes in one turn coalesce into one rebuild";
  const auto root = host.reconciler().root();
  ASSERT_EQ(root->children().size(), 1u);
  EXPECT_EQ(std::static_pointer_cast<TextLayer>(root->children()[0])->text(), "third");
  (void)binding;
}

TEST(UiReconciler, ElementHostReportsLifecycle) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  int mounted = 0;
  int updated = 0;
  int unmounted = 0;
  coral::observer::Property<std::string> title("a");
  {
    ElementHost host(*window, [&] {
      return std::make_shared<StackElement>(Axis::Column,
        std::vector<ElementPtr>{
          std::make_shared<TextElement>(title.get(), font, Color::White(), "title") },
        "root");
    });
    host.setOnMounted([&] { ++mounted; });
    host.setOnUpdated([&] { ++updated; });
    host.setOnUnmounted([&] { ++unmounted; });
    host.mount();
    EXPECT_EQ(mounted, 1);
    auto binding = host.bind(title);
    title.set("b");
    window->pump();
    EXPECT_EQ(updated, 1);
    (void)binding;
  }
  EXPECT_EQ(unmounted, 1);
}

TEST(UiReconciler, ScrollMountsContentAndKeepsTheViewport) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  const ElementPtr first = components::Scroll(
    std::make_shared<TextElement>("inside", font, Color::White(), "text"), 120.0f, "scroll");
  Reconciler reconciler;
  const auto root = reconciler.mount(first);
  const auto scroll = std::dynamic_pointer_cast<ScrollView>(root);
  ASSERT_NE(scroll, nullptr);
  EXPECT_FLOAT_EQ(scroll->preferredHeight(), 120.0f);
  ASSERT_NE(scroll->content(), nullptr);
  EXPECT_EQ(std::static_pointer_cast<TextLayer>(scroll->content())->text(), "inside");

  const ElementPtr second = components::Scroll(
    std::make_shared<TextElement>("after", font, Color::White(), "text"), 120.0f, "scroll");
  reconciler.update(second);
  EXPECT_EQ(reconciler.root(), scroll) << "the viewport layer must survive a props update";
  ASSERT_NE(scroll->content(), nullptr);
  EXPECT_EQ(std::static_pointer_cast<TextLayer>(scroll->content())->text(), "after");
}

TEST(UiReconciler, PropsBackgroundPaintsBehindTheChild) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  auto stack = std::make_shared<StackElement>(Axis::Column,
    std::vector<ElementPtr>{ std::make_shared<TextElement>("label", font, Color::White(), "t") },
    "stack");
  ElementProps props;
  props.background = Color::MakeRGBA8(255, 0, 0, 255);
  Reconciler reconciler;
  const auto root = reconciler.mount(components::WithProps(std::move(stack), props, "bg"));
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->backgroundColor(), Color::MakeRGBA8(255, 0, 0, 255));

  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const auto pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  const uint32_t corner = pixels.getPixelARGB32(4, 4);
  EXPECT_EQ((corner >> 16) & 0xFFu, 255u) << "the background fills outside the text";
}

TEST(UiReconciler, EmptyWhenBranchDoesNotMountALayer) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  auto makeTree = [&](const bool show) {
    return std::make_shared<StackElement>(Axis::Column,
      std::vector<ElementPtr>{
        components::When(show,
          std::make_shared<TextElement>("branch", font, Color::White(), "text"), {}, "when"),
        std::make_shared<ColorElement>(Color::MakeRGBA8(0, 255, 0, 255), "color"),
      },
      "root");
  };
  Reconciler reconciler;
  const auto root = reconciler.mount(makeTree(false));
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->children().size(), 1u) << "a false When must not mount an empty layer";

  reconciler.update(makeTree(true));
  EXPECT_EQ(root->children().size(), 2u);
  reconciler.update(makeTree(false));
  EXPECT_EQ(root->children().size(), 1u);
}

TEST(UiReconciler, ContentViewElementMountsAndSwapsItsHost) {
  auto firstHost = std::make_shared<ContentHost>();
  auto secondHost = std::make_shared<ContentHost>();
  Reconciler reconciler;
  const auto root = reconciler.mount(components::ContentView(firstHost,
    Color::MakeRGBA8(10, 20, 30, 255), "preview"));
  const auto view = std::dynamic_pointer_cast<ContentView>(root);
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view->host(), firstHost);

  reconciler.update(components::ContentView(secondHost, Color::MakeRGBA8(10, 20, 30, 255), "preview"));
  EXPECT_EQ(reconciler.root(), root) << "the ContentView layer must survive a host swap";
  EXPECT_EQ(view->host(), secondHost);
}

TEST(UiReconciler, ButtonEnabledPropertyUpdatesInPlace) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  auto enabled = std::make_shared<coral::observer::Property<bool>>(true);
  Reconciler reconciler;
  const auto root = reconciler.mount(components::Button("Run", font, [] {}, "button", true, enabled));
  const auto button = std::static_pointer_cast<Button>(root);
  ASSERT_NE(button, nullptr);
  EXPECT_TRUE(button->enabled());

  enabled->set(false);
  EXPECT_FALSE(button->enabled()) << "the element bound a live enabled property";

  reconciler.update(components::Button("Run", font, [] {}, "button", true, enabled));
  EXPECT_EQ(reconciler.root(), root);
  EXPECT_FALSE(button->enabled());
}

TEST(UiReconciler, GridMountsChildrenAndUpdatesColumns) {
  const auto makeTree = [](const int columns, const int count) {
    std::vector<ElementPtr> children;
    for (int i = 0; i < count; ++i) {
      children.push_back(std::make_shared<ColorElement>(Color::MakeRGBA8(10, 20, 30, 255),
        "cell-" + std::to_string(i)));
    }
    return components::Grid(columns, std::move(children), "grid", 4.0f);
  };
  Reconciler reconciler;
  const auto root = reconciler.mount(makeTree(2, 4));
  const auto grid = std::dynamic_pointer_cast<Grid>(root);
  ASSERT_NE(grid, nullptr);
  EXPECT_EQ(grid->columns(), 2);
  EXPECT_EQ(grid->children().size(), 4u);

  reconciler.update(makeTree(3, 5));
  EXPECT_EQ(reconciler.root(), grid) << "the grid layer must survive a column/child update";
  EXPECT_EQ(grid->columns(), 3);
  EXPECT_EQ(grid->children().size(), 5u);
}

TEST(UiReconciler, KeyedReorderOnlyReparentsChangedChildren) {
  const auto makeTree = [](std::vector<std::string> order) {
    std::vector<ElementPtr> children;
    for (const std::string& key: order) {
      children.push_back(std::make_shared<ColorElement>(Color::MakeRGBA8(10, 20, 30, 255), key));
    }
    return std::make_shared<StackElement>(Axis::Column, std::move(children), "root");
  };
  Reconciler reconciler;
  const auto root = reconciler.mount(makeTree({ "a", "b", "c" }));
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->children().size(), 3u);
  const auto a = root->children()[0];
  const auto b = root->children()[1];
  const auto c = root->children()[2];
  const uint64_t aBefore = a->reparentCount();
  const uint64_t bBefore = b->reparentCount();
  const uint64_t cBefore = c->reparentCount();

  reconciler.update(makeTree({ "a", "c", "b" }));
  ASSERT_EQ(root->children().size(), 3u);
  EXPECT_EQ(root->children()[0], a);
  EXPECT_EQ(root->children()[1], c);
  EXPECT_EQ(root->children()[2], b);
  EXPECT_EQ(a->reparentCount(), aBefore) << "the unchanged prefix must not be re-attached";
  EXPECT_EQ(c->reparentCount(), cBefore + 2) << "the moved child detaches and re-attaches once";
  EXPECT_EQ(b->reparentCount(), bBefore)
    << "the reconciler moves as few children as possible, so shifting one slot is not a reparent";
}

TEST(UiReconciler, InsertingAKeyedChildKeepsTheTrailingChildrenAttached) {
  const auto makeTree = [](const std::vector<std::string>& order) {
    std::vector<ElementPtr> children;
    for (const std::string& key: order) {
      children.push_back(std::make_shared<ColorElement>(Color::MakeRGBA8(10, 20, 30, 255), key));
    }
    return std::make_shared<StackElement>(Axis::Column, std::move(children), "root");
  };
  Reconciler reconciler;
  const auto root = reconciler.mount(makeTree({ "a", "b", "c" }));
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->children().size(), 3u);
  const auto b = root->children()[1];
  const auto c = root->children()[2];
  const uint64_t bBefore = b->reparentCount();
  const uint64_t cBefore = c->reparentCount();

  reconciler.update(makeTree({ "a", "x", "b", "c" }));
  ASSERT_EQ(root->children().size(), 4u);
  EXPECT_EQ(root->children()[2], b);
  EXPECT_EQ(root->children()[3], c);
  EXPECT_EQ(b->reparentCount(), bBefore) << "trailing children must keep their layer identity";
  EXPECT_EQ(c->reparentCount(), cBefore);
}

TEST(UiReconciler, PropsElementComparesItsPropsByValue) {
  const auto make = [](const float opacity) {
    ElementProps props;
    props.opacity = opacity;
    props.layout.grow = 1.0f;
    return components::WithProps(
      std::make_shared<ColorElement>(Color::MakeRGBA8(1, 2, 3, 255), "fill"), props, "props");
  };
  const ElementPtr first = make(0.5f);
  const ElementPtr same = make(0.5f);
  const ElementPtr changed = make(0.25f);
  EXPECT_TRUE(first->measureEquivalent(*same));
  EXPECT_FALSE(first->measureEquivalent(*changed));
}

TEST(UiReconciler, DockDropsAPanelWhoseBranchBecomesFalse) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  const auto makeTree = [&](const bool showLeft) {
    return components::Dock(
      std::make_shared<TextElement>("center", font, Color::White(), "c"),
      components::When(showLeft,
        std::make_shared<TextElement>("left", font, Color::White(), "l")),
      {}, {}, 180.0f, 220.0f, 140.0f, "dock");
  };
  Reconciler reconciler;
  const auto root = reconciler.mount(makeTree(true));
  const auto dock = std::dynamic_pointer_cast<DockView>(root);
  ASSERT_NE(dock, nullptr);
  ASSERT_NE(dock->panel(DockView::Region::Left), nullptr);

  reconciler.update(makeTree(false));
  EXPECT_EQ(dock->panel(DockView::Region::Left), nullptr)
    << "a region whose When branch resolved to null must not keep its old panel";
  EXPECT_NE(dock->panel(DockView::Region::Center), nullptr);
}

TEST(UiReconciler, ScrollDropsContentWhoseBranchBecomesFalse) {
  const auto makeTree = [](const bool present) {
    return components::Scroll(
      components::When(present,
        std::make_shared<ColorElement>(Color::MakeRGBA8(5, 6, 7, 255), "body")),
      120.0f, "scroll");
  };
  Reconciler reconciler;
  const auto root = reconciler.mount(makeTree(true));
  const auto scroll = std::dynamic_pointer_cast<ScrollView>(root);
  ASSERT_NE(scroll, nullptr);
  ASSERT_NE(scroll->content(), nullptr);

  reconciler.update(makeTree(false));
  EXPECT_EQ(scroll->content(), nullptr);
}

TEST(UiReconciler, DockMountsRegionsAndPatchesOneInPlace) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  const auto makeTree = [&](const std::string& leftText) {
    return components::Dock(
      std::make_shared<TextElement>("center", font, Color::White(), "c"),
      std::make_shared<TextElement>(leftText, font, Color::White(), "l"),
      {}, {}, 180.0f, 220.0f, 140.0f, "dock");
  };
  Reconciler reconciler;
  const auto root = reconciler.mount(makeTree("before"));
  const auto dock = std::dynamic_pointer_cast<DockView>(root);
  ASSERT_NE(dock, nullptr);
  ASSERT_NE(dock->panel(DockView::Region::Center), nullptr);
  const auto center = dock->panel(DockView::Region::Center);
  const auto left = dock->panel(DockView::Region::Left);
  ASSERT_NE(left, nullptr);
  EXPECT_FLOAT_EQ(dock->size(DockView::Region::Left), 180.0f);
  EXPECT_FLOAT_EQ(dock->size(DockView::Region::Right), 220.0f);
  EXPECT_FLOAT_EQ(dock->size(DockView::Region::Bottom), 140.0f);

  reconciler.update(makeTree("after"));
  EXPECT_EQ(reconciler.root(), dock);
  EXPECT_EQ(dock->panel(DockView::Region::Center), center) << "an untouched region keeps its panel";
  const auto leftAfter = dock->panel(DockView::Region::Left);
  ASSERT_NE(leftAfter, nullptr);
  EXPECT_EQ(leftAfter, left) << "the region is patched in place";
  EXPECT_EQ(std::static_pointer_cast<TextLayer>(leftAfter)->text(), "after");
}

TEST(UiReconciler, SliderMountsAndPatchesItsValue) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  Reconciler reconciler;
  const auto root = reconciler.mount(components::Slider("Opacity", font, 0.0, 1.0, 0.25, {}, "opacity"));
  const auto slider = std::dynamic_pointer_cast<Slider>(root);
  ASSERT_NE(slider, nullptr);
  EXPECT_EQ(slider->label(), "Opacity");
  EXPECT_DOUBLE_EQ(slider->value(), 0.25);

  reconciler.update(components::Slider("Opacity", font, 0.0, 1.0, 0.75, {}, "opacity"));
  EXPECT_EQ(reconciler.root(), slider);
  EXPECT_DOUBLE_EQ(slider->value(), 0.75);
}

TEST(UiReconciler, PropsApplyCornerRadiusAndShadow) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  auto stack = std::make_shared<StackElement>(Axis::Column,
    std::vector<ElementPtr>{ std::make_shared<TextElement>("label", font, Color::White(), "t") },
    "stack");
  Shadow shadow;
  shadow.blurRadius = 6.0f;
  shadow.color = Color::MakeRGBA8(0, 0, 0, 120);
  ElementProps props;
  props.clip = true;
  props.cornerRadius = 8.0f;
  props.shadow = shadow;
  Reconciler reconciler;
  const auto root = reconciler.mount(components::WithProps(std::move(stack), props, "panel"));
  ASSERT_NE(root, nullptr);
  EXPECT_FLOAT_EQ(root->clipRadius(), 8.0f);
  ASSERT_TRUE(root->hasShadow());
  EXPECT_FLOAT_EQ(root->shadow().blurRadius, 6.0f);

  ElementProps plain;
  plain.clip = true;
  stack = std::make_shared<StackElement>(Axis::Column,
    std::vector<ElementPtr>{ std::make_shared<TextElement>("label", font, Color::White(), "t") },
    "stack");
  reconciler.update(components::WithProps(std::move(stack), plain, "panel"));
  EXPECT_FALSE(root->hasShadow()) << "an unset shadow clears the previous one";
}

TEST(UiReconciler, PropsBackgroundRoleFollowsTheTheme) {
  const auto font = testFont();
  ASSERT_NE(font, nullptr);
  auto stack = std::make_shared<StackElement>(Axis::Column,
    std::vector<ElementPtr>{ std::make_shared<TextElement>("label", font, Color::White(), "t") },
    "stack");
  ElementProps props;
  props.backgroundRole = ThemeColorRole::Surface;
  Reconciler reconciler;
  const auto root = reconciler.mount(components::WithProps(std::move(stack), props, "panel"));
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->backgroundColorRole(), ThemeColorRole::Surface);

  const auto dark = std::make_shared<Theme>(Theme::dark());
  root->setTheme(dark);
  EXPECT_EQ(root->backgroundColor(), dark->surface);

  const auto light = std::make_shared<Theme>(Theme::light());
  root->setTheme(light);
  EXPECT_EQ(root->backgroundColor(), light->surface) << "a theme switch re-resolves the token";
}

TEST(UiReconciler, LayerElementHostsAPrebuiltWidget) {
  auto widget = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  Reconciler reconciler;
  const auto root = reconciler.mount(components::Layer(widget, "widget"));
  EXPECT_EQ(root, widget) << "the hosted layer is used as-is";

  reconciler.update(components::Layer(widget, "widget"));
  EXPECT_EQ(reconciler.root(), widget);

  auto replacement = std::make_shared<ColorLayer>(Color::MakeRGBA8(4, 5, 6, 255));
  reconciler.update(components::Layer(replacement, "widget"));
  EXPECT_EQ(reconciler.root(), replacement);
}

TEST(UiReconciler, MountsTheCommonControlElements) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto build = [&font](const bool checked, const double progress) {
    return stack({
      components::CheckBox("Sync", font, checked, [](bool) {}, "check"),
      components::Toggle("Live", font, checked, [](bool) {}, "toggle"),
      components::RadioButton("One", font, checked, [] {}, "radio"),
      components::ProgressBar(font, progress, "progress"),
    });
  };

  Reconciler reconciler;
  const auto root = reconciler.mount(build(false, 0.25));
  ASSERT_EQ(root->children().size(), 4u);
  auto check = std::dynamic_pointer_cast<CheckBox>(root->children()[0]);
  auto toggle = std::dynamic_pointer_cast<Toggle>(root->children()[1]);
  auto radio = std::dynamic_pointer_cast<RadioButton>(root->children()[2]);
  auto progress = std::dynamic_pointer_cast<ProgressBar>(root->children()[3]);
  ASSERT_TRUE(check != nullptr);
  ASSERT_TRUE(toggle != nullptr);
  ASSERT_TRUE(radio != nullptr);
  ASSERT_TRUE(progress != nullptr);
  EXPECT_FALSE(check->checked());
  EXPECT_FALSE(toggle->on());
  EXPECT_FALSE(radio->selected());
  EXPECT_DOUBLE_EQ(progress->value(), 0.25);

  const auto checkLayer = root->children()[0];
  reconciler.update(build(true, 0.75));
  EXPECT_EQ(root->children()[0], checkLayer) << "the control layer is patched, not replaced";
  EXPECT_TRUE(check->checked());
  EXPECT_TRUE(toggle->on());
  EXPECT_TRUE(radio->selected());
  EXPECT_DOUBLE_EQ(progress->value(), 0.75);
}

TEST(UiReconciler, RegisteredElementsBuildAndPatchByName) {
  ElementRegistry registry;
  int builds = 0;
  EXPECT_TRUE(registry.add("Badge", [&builds](const ElementProps&, std::vector<ElementPtr>) {
    ++builds;
    return std::make_shared<ColorElement>(Color::MakeRGBA8(9, 9, 9, 255), "badge");
  }));
  EXPECT_FALSE(registry.add("", [](const ElementProps&, std::vector<ElementPtr>) {
    return ElementPtr{};
  })) << "an empty type name is rejected";
  EXPECT_TRUE(registry.contains("Badge"));

  EXPECT_EQ(registry.types().size(), 1u);
  EXPECT_EQ(registry.build("Missing"), nullptr);

  Reconciler reconciler;
  ASSERT_NE(reconciler.mount(registry.build("Badge", {}, {}, "keyed")), nullptr);
  EXPECT_EQ(builds, 1);
  reconciler.update(registry.build("Badge", {}, {}, "keyed"));
  EXPECT_EQ(builds, 2) << "a registry build runs on every reconcile";

  ElementProps defaults;
  defaults.opacity = 0.5f;
  defaults.cornerRadius = 6.0f;
  EXPECT_TRUE(registry.add("Card", [](const ElementProps&, std::vector<ElementPtr>) {
    return std::make_shared<ColorElement>(Color::MakeRGBA8(20, 30, 40, 255), "card");
  }, defaults));
  Reconciler withDefaults;
  const auto card = withDefaults.mount(registry.build("Card"));
  ASSERT_NE(card, nullptr);
  EXPECT_FLOAT_EQ(card->opacity(), 0.5f) << "registered defaults apply";
  EXPECT_FLOAT_EQ(card->clipRadius(), 6.0f);

  ElementProps overridden;
  overridden.opacity = 0.9f;
  withDefaults.update(registry.build("Card", overridden));
  EXPECT_FLOAT_EQ(withDefaults.root()->opacity(), 0.9f) << "caller props win over defaults";
}

TEST(UiReconciler, MountsAndPatchesASegmentedControl) {
  const auto font = testFont();
  if (font == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto build = [&font](const int selected) {
    return stack({ components::SegmentedControl({ "One", "Two", "Three" }, font, selected,
      [](int) {}, "segments") });
  };

  Reconciler reconciler;
  const auto root = reconciler.mount(build(0));
  auto control = std::dynamic_pointer_cast<SegmentedControl>(root->children()[0]);
  ASSERT_TRUE(control != nullptr);
  EXPECT_EQ(control->segments().size(), 3u);
  EXPECT_EQ(control->selectedIndex(), 0);

  const auto layer = root->children()[0];
  reconciler.update(build(2));
  EXPECT_EQ(root->children()[0], layer) << "the segmented control is patched in place";
  EXPECT_EQ(control->selectedIndex(), 2);
}

TEST(UiReconciler, LayoutTransitionAnimatesBoundsChanges) {
  ElementProps props;
  props.bounds = Rect::MakeWH(10.0f, 10.0f);
  props.layoutTransition = std::chrono::milliseconds(150);
  auto build = [&props] {
    return stack({ components::WithProps(
      std::make_shared<ColorElement>(Color::MakeRGBA8(3, 4, 5, 255), "inner"), props, "wrapper") });
  };

  Reconciler reconciler;
  const auto root = reconciler.mount(build());
  const auto layer = root->children()[0];
  props.bounds = Rect::MakeXYWH(100.0f, 100.0f, 60.0f, 60.0f);
  reconciler.update(build());
  ASSERT_EQ(root->children()[0], layer) << "the wrapper layer is reused";
  EXPECT_TRUE(layer->animatingBounds()) << "the bounds change starts an animation";

  layer->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(400));
  EXPECT_FALSE(layer->animatingBounds());
  EXPECT_FLOAT_EQ(layer->bounds().getLeft(), 100.0f);
  EXPECT_FLOAT_EQ(layer->bounds().getWidth(), 60.0f);

  // Without a transition (or with a zero one) the setter keeps snapping.
  props.layoutTransition = std::chrono::milliseconds(0);
  props.bounds = Rect::MakeWH(20.0f, 20.0f);
  reconciler.update(build());
  EXPECT_FALSE(layer->animatingBounds());
  EXPECT_FLOAT_EQ(layer->bounds().getWidth(), 20.0f);
}

TEST(UiReconciler, MountsAndPatchesATable) {
  auto first = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 0, 0, 255));
  auto second = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 10, 0, 255));
  auto third = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 0, 10, 255));
  const std::vector<TableColumn> columns{ TableColumn{ 0.0f, 1.0f }, TableColumn{ 0.0f, 1.0f } };

  const auto build = [&](std::shared_ptr<Layer> cell) {
    return stack({ components::Table(columns, { { cell, second } }, 4.0f, Insets{}, "table") });
  };
  Reconciler reconciler;
  const auto root = reconciler.mount(build(first));
  auto table = std::dynamic_pointer_cast<Table>(root->children()[0]);
  ASSERT_TRUE(table != nullptr);
  EXPECT_EQ(table->columns().size(), 2u);
  ASSERT_EQ(table->rows().size(), 1u);
  EXPECT_EQ(table->rows()[0][0], first);

  const auto layer = root->children()[0];
  reconciler.update(build(third));
  EXPECT_EQ(root->children()[0], layer) << "the table layer is patched in place";
  ASSERT_EQ(table->rows().size(), 1u);
  EXPECT_EQ(table->rows()[0][0], third);
  EXPECT_EQ(table->rows()[0][1], second);
}

TEST(UiReconciler, MountsAndPatchesAnIconButton) {
  Reconciler reconciler;
  const auto root = reconciler.mount(components::IconButton(nullptr, "Play", [] {}, "icon"));
  auto button = std::dynamic_pointer_cast<IconButton>(root);
  ASSERT_TRUE(button != nullptr) << "the icon button element creates an IconButton layer";
  EXPECT_EQ(button->icon(), nullptr);

  reconciler.update(components::IconButton(nullptr, "Pause", [] {}, "icon"));
  EXPECT_EQ(reconciler.root(), root) << "the icon button layer is patched in place";
}

TEST(UiReconciler, MountsAndPatchesAColorButton) {
  Reconciler reconciler;
  const auto root = reconciler.mount(components::ColorButton(Color::MakeRGBA8(10, 20, 30, 255),
    "Accent", [] {}, "swatch"));
  auto button = std::dynamic_pointer_cast<ColorButton>(root);
  ASSERT_TRUE(button != nullptr) << "the colour button element creates a ColorButton layer";
  EXPECT_EQ(button->color().toR8G8B8A8()[0], 10);

  reconciler.update(components::ColorButton(Color::MakeRGBA8(40, 50, 60, 255), "Accent", [] {},
    "swatch"));
  EXPECT_EQ(reconciler.root(), root) << "the swatch layer is patched in place";
  EXPECT_EQ(button->color().toR8G8B8A8()[0], 40);
}

TEST(UiReconciler, SharedElementTagAnimatesFromTheRememberedBounds) {
  ElementProps first;
  first.bounds = Rect::MakeXYWH(0.0f, 0.0f, 40.0f, 40.0f);
  first.sharedElementTag = "hero";
  Reconciler reconciler;
  const auto root = reconciler.mount(stack({ components::WithProps(
    std::make_shared<ColorElement>(Color::MakeRGBA8(1, 2, 3, 255), "hero"), first, "a") }));
  ASSERT_NE(root, nullptr);

  reconciler.update(stack({}));
  EXPECT_TRUE(reconciler.root()->children().empty());

  ElementProps second;
  second.bounds = Rect::MakeXYWH(200.0f, 100.0f, 80.0f, 80.0f);
  second.sharedElementTag = "hero";
  reconciler.update(stack({ components::WithProps(
    std::make_shared<ColorElement>(Color::MakeRGBA8(1, 2, 3, 255), "hero"), second, "b") }));
  const auto out = reconciler.root()->children().front();
  ASSERT_NE(out, nullptr);
  EXPECT_TRUE(out->animatingBounds()) << "the tagged element starts from the remembered bounds";
  EXPECT_FLOAT_EQ(out->bounds().getLeft(), 0.0f) << "it starts where the old element was";

  out->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(500));
  EXPECT_FLOAT_EQ(out->bounds().getLeft(), 200.0f) << "and finishes at its own bounds";
  EXPECT_FLOAT_EQ(out->bounds().getWidth(), 80.0f);
}

TEST(UiReconciler, SharedElementDurationIsConfigurable) {
  ElementProps first;
  first.bounds = Rect::MakeXYWH(0.0f, 0.0f, 40.0f, 40.0f);
  first.sharedElementTag = "hero";
  Reconciler reconciler;
  reconciler.setSharedElementDuration(std::chrono::milliseconds(100));
  EXPECT_EQ(reconciler.sharedElementDuration(), std::chrono::milliseconds(100));
  reconciler.mount(stack({ components::WithProps(
    std::make_shared<ColorElement>(Color::MakeRGBA8(1, 2, 3, 255), "hero"), first, "a") }));
  reconciler.update(stack({}));

  ElementProps second;
  second.bounds = Rect::MakeXYWH(200.0f, 0.0f, 40.0f, 40.0f);
  second.sharedElementTag = "hero";
  reconciler.update(stack({ components::WithProps(
    std::make_shared<ColorElement>(Color::MakeRGBA8(1, 2, 3, 255), "hero"), second, "b") }));
  const auto out = reconciler.root()->children().front();
  ASSERT_TRUE(out->animatingBounds());
  out->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(150));
  EXPECT_FLOAT_EQ(out->bounds().getLeft(), 200.0f)
    << "the configured 100 ms transition has finished by 150 ms";
}

TEST(UiReconciler, MountsASpinnerAndAdvancesItsPhase) {
  Reconciler reconciler;
  const auto root = reconciler.mount(components::Spinner(true, Color::MakeRGBA8(200, 100, 50, 255),
    "busy"));
  auto spinner = std::dynamic_pointer_cast<Spinner>(root);
  ASSERT_TRUE(spinner != nullptr);
  EXPECT_TRUE(spinner->spinning());
  EXPECT_TRUE(spinner->hasRunningAnimation()) << "a spinning indicator keeps the frame loop alive";

  const float before = spinner->phase();
  spinner->onFrame(std::chrono::steady_clock::now());
  spinner->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(200));
  EXPECT_NE(spinner->phase(), before) << "frames advance the dot phase";

  reconciler.update(components::Spinner(false, Color::MakeRGBA8(200, 100, 50, 255), "busy"));
  EXPECT_EQ(reconciler.root(), root) << "the spinner layer is patched in place";
  EXPECT_FALSE(spinner->spinning());
  EXPECT_FALSE(spinner->hasRunningAnimation());
}