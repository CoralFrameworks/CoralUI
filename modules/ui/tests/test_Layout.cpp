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
#include <ui/Flex.h>
#include <graphics/FontFallback.h>
#include <ui/Accessibility.h>
#include <ui/DockView.h>
#include <ui/Grid.h>
#include <ui/Layer.h>
#include <ui/ScrollView.h>
#include <ui/Table.h>
#include <ui/TableView.h>
#include <ui/TextLayer.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

using namespace coral::ui;

/** A leaf with a fixed preferred size, which is what a row/column test needs. */
class FixedLayer final : public Layer {
 public:
  FixedLayer(const float width, const float height) : mSize{ width, height } {}

  Size measure(const Size& available) override {
    (void)available;
    setMeasuredSize(mSize);
    return mSize;
  }

  bool onPointerDown(const PointerEvent& event) override {
    (void)event;
    ++clicks;
    return true;
  }

  int clicks = 0;

 private:
  Size mSize;
};

TEST(UiLayout, ARowAddsPaddingAndGap) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setPadding({ 10.0f, 10.0f, 10.0f, 10.0f });
  row->setGap(5.0f);
  row->addChild(std::make_shared<FixedLayer>(30.0f, 20.0f));
  row->addChild(std::make_shared<FixedLayer>(40.0f, 30.0f));

  const Size measured = row->measure({ 200.0f, 100.0f });
  EXPECT_FLOAT_EQ(measured.width, 95.0f);
  EXPECT_FLOAT_EQ(measured.height, 50.0f);
}

TEST(UiLayout, ARowPlacesItsChildrenLeftToRight) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setPadding({ 10.0f, 10.0f, 10.0f, 10.0f });
  row->setGap(5.0f);
  auto first = std::make_shared<FixedLayer>(30.0f, 20.0f);
  auto second = std::make_shared<FixedLayer>(40.0f, 30.0f);
  row->addChild(first);
  row->addChild(second);

  row->measure({ 200.0f, 100.0f });
  row->setBounds(Rect::MakeWH(95.0f, 50.0f));
  row->layout();

  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 10.0f);
  EXPECT_FLOAT_EQ(first->bounds().getTop(), 10.0f);
  EXPECT_FLOAT_EQ(first->bounds().getWidth(), 30.0f);
  EXPECT_FLOAT_EQ(second->bounds().getLeft(), 45.0f);
  EXPECT_FLOAT_EQ(second->bounds().getTop(), 10.0f);
  EXPECT_FLOAT_EQ(second->bounds().getWidth(), 40.0f);
}

TEST(UiLayout, CrossAxisCenterCentresTheSmallerChild) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setAxis(Axis::Row);
  row->setCrossAlign(Align::Center);
  auto small = std::make_shared<FixedLayer>(20.0f, 10.0f);
  auto tall = std::make_shared<FixedLayer>(20.0f, 30.0f);
  row->addChild(small);
  row->addChild(tall);

  row->measure({ 100.0f, 100.0f });
  row->setBounds(Rect::MakeWH(40.0f, 30.0f));
  row->layout();

  EXPECT_FLOAT_EQ(small->bounds().getTop(), 10.0f);
  EXPECT_FLOAT_EQ(tall->bounds().getTop(), 0.0f);
}

TEST(UiLayout, AColumnStacksVertically) {
  auto column = std::make_shared<Flex>(Axis::Column);
  column->setGap(4.0f);
  auto first = std::make_shared<FixedLayer>(20.0f, 10.0f);
  auto second = std::make_shared<FixedLayer>(30.0f, 20.0f);
  column->addChild(first);
  column->addChild(second);

  const Size measured = column->measure({ 100.0f, 100.0f });
  EXPECT_FLOAT_EQ(measured.width, 30.0f);
  EXPECT_FLOAT_EQ(measured.height, 34.0f);
  column->setBounds(Rect::MakeWH(30.0f, 34.0f));
  column->layout();
  EXPECT_FLOAT_EQ(second->bounds().getTop(), 14.0f);
}

TEST(UiLayout, RowGrowsChildrenWithTheirWeights) {
  auto row = std::make_shared<Flex>(Axis::Row);
  auto first = std::make_shared<FixedLayer>(50.0f, 20.0f);
  auto second = std::make_shared<FixedLayer>(50.0f, 20.0f);
  LayoutParams firstParams;
  firstParams.grow = 1.0f;
  LayoutParams secondParams;
  secondParams.grow = 2.0f;
  first->setLayoutParams(firstParams);
  second->setLayoutParams(secondParams);
  row->addChild(first);
  row->addChild(second);
  row->measure({ 300.0f, 100.0f });
  row->setBounds(Rect::MakeWH(300.0f, 100.0f));
  row->layout();
  EXPECT_NEAR(first->bounds().getWidth(), 50.0f + 200.0f / 3.0f, 0.01f);
  EXPECT_NEAR(second->bounds().getWidth(), 50.0f + 400.0f / 3.0f, 0.01f);
  EXPECT_NEAR(second->bounds().getLeft(), 50.0f + 200.0f / 3.0f, 0.01f);
}

TEST(UiLayout, PercentageAndMarginsAreResolvedBeforeLayout) {
  auto row = std::make_shared<Flex>(Axis::Row);
  auto child = std::make_shared<FixedLayer>(10.0f, 10.0f);
  LayoutParams params;
  params.widthPercent = 0.25f;
  params.heightPercent = 0.5f;
  params.margin.left = 10.0f;
  params.margin.right = 10.0f;
  child->setLayoutParams(params);
  row->addChild(child);
  row->measure({ 300.0f, 100.0f });
  row->setBounds(Rect::MakeWH(300.0f, 100.0f));
  row->layout();
  EXPECT_NEAR(child->bounds().getWidth(), 75.0f, 0.01f);
  EXPECT_NEAR(child->bounds().getHeight(), 50.0f, 0.01f);
  EXPECT_NEAR(child->bounds().getLeft(), 10.0f, 0.01f);
}

TEST(UiLayout, RowShrinksOverflowingChildren) {
  auto row = std::make_shared<Flex>(Axis::Row);
  auto first = std::make_shared<FixedLayer>(80.0f, 20.0f);
  auto second = std::make_shared<FixedLayer>(80.0f, 20.0f);
  row->addChild(first);
  row->addChild(second);
  row->measure({ 100.0f, 100.0f });
  row->setBounds(Rect::MakeWH(100.0f, 100.0f));
  row->layout();
  EXPECT_NEAR(first->bounds().getWidth(), 50.0f, 0.01f);
  EXPECT_NEAR(second->bounds().getWidth(), 50.0f, 0.01f);
}

TEST(UiLayout, GridPlacesChildrenInRowsAndColumns) {
  auto grid = std::make_shared<Grid>(2);
  grid->setPadding({ 5.0f, 5.0f, 5.0f, 5.0f });
  grid->setGap(4.0f);
  auto first = std::make_shared<FixedLayer>(40.0f, 20.0f);
  auto second = std::make_shared<FixedLayer>(40.0f, 20.0f);
  auto third = std::make_shared<FixedLayer>(40.0f, 20.0f);
  grid->addChild(first);
  grid->addChild(second);
  grid->addChild(third);
  const Size measured = grid->measure({ 100.0f, 100.0f });
  EXPECT_FLOAT_EQ(measured.width, 100.0f);
  grid->setBounds(Rect::MakeWH(100.0f, measured.height));
  grid->layout();
  EXPECT_NEAR(first->bounds().getLeft(), 5.0f, 0.01f);
  EXPECT_NEAR(second->bounds().getLeft(), 5.0f + 43.0f + 4.0f, 0.01f);
  EXPECT_NEAR(third->bounds().getTop(), 5.0f + 20.0f + 4.0f, 0.01f);
}

TEST(UiLayout, AGridCellIsNotAFixedParentSize) {
  // The column is the grid's, but the row is as tall as its tallest measured cell -- so a change inside
  // a cell can change the grid's rows. Marking a cell as parent-assigned would stop that propagation and
  // leave a stale row height, which is why only the containers that size both axes from their own
  // geometry (Flex, DockView) do it.
  auto grid = std::make_shared<Grid>(2);
  grid->setGap(4.0f);
  auto cell = std::make_shared<Flex>(Axis::Column);
  auto label = std::make_shared<FixedLayer>(20.0f, 18.0f);
  cell->addChild(label);
  grid->addChild(cell);
  grid->addChild(std::make_shared<FixedLayer>(40.0f, 20.0f));
  grid->measure({ 100.0f, 100.0f });
  grid->setBounds(Rect::MakeWH(100.0f, 60.0f));
  grid->layout();

  EXPECT_FALSE(cell->parentAssignedSize())
    << "a grid row comes from the cells' measurements, so the cell is not fixed by the grid";
  EXPECT_FALSE(cell->hasFixedParentSize());
  cell->setNeedsLayout();
  EXPECT_TRUE(grid->subtreeNeedsLayout()) << "a cell's layout change has to reach the grid";
}

TEST(UiLayout, ATableRowIsAsTallAsItsTallestMeasuredCell) {
  // The same rule as a grid cell: the column width is the table's, the row height is the cells'. A cell
  // that was marked as fixed would keep the old row height after its content grew.
  auto table = std::make_shared<Table>();
  table->setColumns({ TableColumn{ 0.0f, 1.0f, 0.0f }, TableColumn{ 0.0f, 1.0f, 0.0f } });
  auto cell = std::make_shared<Flex>(Axis::Column);
  auto taller = std::make_shared<FixedLayer>(20.0f, 30.0f);
  cell->addChild(taller);
  table->setRows({ { cell, std::make_shared<FixedLayer>(20.0f, 10.0f) } });
  table->measure({ 100.0f, 100.0f });
  table->setBounds(Rect::MakeWH(100.0f, 40.0f));
  table->layout();

  EXPECT_FALSE(cell->parentAssignedSize())
    << "the row height is the tallest measured cell's, so a table cell is not fixed by the table";
  EXPECT_FALSE(cell->hasFixedParentSize());
  cell->setNeedsLayout();
  EXPECT_TRUE(table->subtreeNeedsLayout()) << "a cell's layout change has to reach the table";
}

TEST(UiLayout, GridSupportsColumnAndRowSpans) {
  auto grid = std::make_shared<Grid>(2);
  grid->setGap(4.0f);
  auto wide = std::make_shared<FixedLayer>(40.0f, 20.0f);
  auto tall = std::make_shared<FixedLayer>(40.0f, 40.0f);
  auto afterWide = std::make_shared<FixedLayer>(40.0f, 20.0f);
  auto afterTall = std::make_shared<FixedLayer>(40.0f, 20.0f);
  grid->addChild(wide);
  grid->addChild(tall);
  grid->addChild(afterWide);
  grid->addChild(afterTall);
  grid->setSpan(wide, 2);
  grid->setSpan(tall, 1, 2);

  const Size measured = grid->measure({ 100.0f, 100.0f });
  grid->setBounds(Rect::MakeWH(100.0f, measured.height));
  grid->layout();

  EXPECT_NEAR(wide->bounds().getWidth(), 100.0f, 0.01f);
  EXPECT_NEAR(wide->bounds().getTop(), 0.0f, 0.01f);
  EXPECT_NEAR(afterWide->bounds().getTop(), 24.0f, 0.01f)
    << "the full-width child consumes its own row";
  EXPECT_NEAR(tall->bounds().getHeight(), 44.0f, 0.01f)
    << "the row-spanning child covers two rows and the inter-row gap";
  EXPECT_NEAR(afterTall->bounds().getTop(), 48.0f, 0.01f)
    << "auto-placement skips the cells occupied by the row span";
}

TEST(UiLayout, TransformAndOpacityAffectHitTesting) {
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 50.0f, 30.0f));
  child->setScale(2.0f);
  root->addChild(child);

  // Scaling is around the bounds centre, so (0, 0) is inside the transformed child even though it
  // was outside the untransformed bounds.
  EXPECT_EQ(root->hitTest({ 0.0f, 0.0f }), child.get());
  child->setOpacity(0.0f);
  EXPECT_EQ(root->hitTest({ 20.0f, 20.0f }), root.get());
}

TEST(UiLayout, RoundedClipRejectsCornerHits) {
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeWH(40.0f, 40.0f));
  child->setClip(true);
  child->setClipRadius(12.0f);
  root->addChild(child);

  EXPECT_EQ(root->hitTest({ 20.0f, 20.0f }), child.get());
  EXPECT_EQ(root->hitTest({ 1.0f, 1.0f }), root.get());
}

TEST(UiLayout, LayerAnimationsAdvanceWithTheFrameClock) {
  Layer layer;
  layer.setOpacity(0.0f);
  layer.setTranslation({ 0.0f, 0.0f });
  const auto start = std::chrono::steady_clock::now();
  layer.animateOpacity(1.0f, std::chrono::milliseconds(100), Easing::Linear);
  layer.animateTranslation({ 40.0f, 20.0f }, std::chrono::milliseconds(100), Easing::Linear);
  layer.onFrame(start + std::chrono::milliseconds(50));
  EXPECT_NEAR(layer.opacity(), 0.5f, 0.1f);
  EXPECT_NEAR(layer.translation().x, 20.0f, 5.0f);
  layer.onFrame(start + std::chrono::milliseconds(120));
  EXPECT_FLOAT_EQ(layer.opacity(), 1.0f);
  EXPECT_FLOAT_EQ(layer.translation().x, 40.0f);
}

TEST(UiLayout, ImplicitTransitionsApplyToCompositingSetters) {
  Layer layer;
  layer.setOpacity(1.0f);
  layer.setTransition(std::chrono::milliseconds(100), Easing::Linear);
  const auto start = std::chrono::steady_clock::now();
  layer.setOpacity(0.5f);
  layer.setTranslation({ 50.0f, 0.0f });
  layer.onFrame(start + std::chrono::milliseconds(50));
  EXPECT_NEAR(layer.opacity(), 0.75f, 0.1f);
  EXPECT_NEAR(layer.translation().x, 25.0f, 5.0f);
  layer.onFrame(start + std::chrono::milliseconds(120));
  EXPECT_FLOAT_EQ(layer.opacity(), 0.5f);
  EXPECT_FLOAT_EQ(layer.translation().x, 50.0f);

  layer.setTransition(std::chrono::milliseconds(0));
  layer.setOpacity(1.0f);
  EXPECT_FLOAT_EQ(layer.opacity(), 1.0f);
}

TEST(UiLayout, FlexRespectsMinimumChildSize) {
  auto row = std::make_shared<Flex>(Axis::Row);
  auto child = std::make_shared<FixedLayer>(40.0f, 10.0f);
  LayoutParams params;
  params.minWidth = 50.0f;
  params.minHeight = 36.0f;
  child->setLayoutParams(params);
  row->addChild(child);
  row->measure({ 100.0f, 100.0f });
  row->setBounds(Rect::MakeWH(100.0f, 100.0f));
  row->layout();
  EXPECT_FLOAT_EQ(child->bounds().getWidth(), 50.0f);
  EXPECT_FLOAT_EQ(child->bounds().getHeight(), 36.0f);
}

TEST(UiLayout, HitTestReturnsTheTopmostChild) {
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  auto bottom = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  bottom->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 50.0f, 50.0f));
  auto top = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  top->setBounds(Rect::MakeLTRB(20.0f, 20.0f, 60.0f, 60.0f));
  root->addChild(bottom);
  root->addChild(top);

  EXPECT_EQ(root->hitTest({ 30.0f, 30.0f }), top.get());
  EXPECT_EQ(root->hitTest({ 15.0f, 15.0f }), bottom.get());
  EXPECT_EQ(root->hitTest({ 70.0f, 70.0f }), root.get());
  EXPECT_EQ(root->hitTest({ -1.0f, -1.0f }), nullptr);
}

TEST(UiLayout, APointerEventBubblesToTheFirstHandler) {
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  auto button = std::make_shared<FixedLayer>(40.0f, 20.0f);
  button->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 50.0f, 30.0f));
  root->addChild(button);

  Layer* const hit = root->hitTest({ 15.0f, 15.0f });
  ASSERT_EQ(hit, button.get());
  PointerEvent event;
  event.position = { 15.0f, 15.0f };
  for (Layer* layer = hit; layer != nullptr; layer = layer->parent()) {
    if (layer->onPointerDown(event)) {
      break;
    }
  }
  EXPECT_EQ(button->clicks, 1);
}

TEST(UiLayout, HidingAChildRemovesItFromMeasureAndHitTest) {
  auto root = std::make_shared<Flex>(Axis::Row);
  auto child = std::make_shared<FixedLayer>(40.0f, 20.0f);
  root->addChild(child);
  child->setVisible(false);

  const Size measured = root->measure({ 100.0f, 100.0f });
  EXPECT_FLOAT_EQ(measured.width, 0.0f);
  EXPECT_FLOAT_EQ(measured.height, 0.0f);
  root->setBounds(Rect::MakeWH(100.0f, 100.0f));
  EXPECT_EQ(root->hitTest({ 10.0f, 10.0f }), root.get());
}

TEST(UiLayout, ARowJustifiesItsItems) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setPadding({ 10.0f, 0.0f, 10.0f, 0.0f });
  auto first = std::make_shared<FixedLayer>(20.0f, 10.0f);
  auto second = std::make_shared<FixedLayer>(20.0f, 10.0f);
  row->addChild(first);
  row->addChild(second);

  row->measure({ 200.0f, 50.0f });
  row->setBounds(Rect::MakeWH(200.0f, 50.0f));

  row->setJustify(Justify::Center);
  row->layout();
  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 80.0f)
    << "(180 - 40) / 2 of inner space before the pair, plus the 10 padding";

  row->setJustify(Justify::End);
  row->layout();
  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 150.0f);

  row->setJustify(Justify::SpaceBetween);
  row->layout();
  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 10.0f);
  EXPECT_FLOAT_EQ(second->bounds().getLeft(), 170.0f);

  row->setJustify(Justify::SpaceEvenly);
  row->layout();
  // Three equal gaps across 140 free pixels: one before, one between, one after.
  EXPECT_NEAR(first->bounds().getLeft(), 10.0f + 140.0f / 3.0f, 0.01f);
  EXPECT_NEAR(second->bounds().getLeft(), 10.0f + 2.0f * 140.0f / 3.0f + 20.0f, 0.01f);
}

TEST(UiLayout, AWrappedRowStacksLinesOnTheCrossAxis) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setWrap(true);
  row->setGap(0.0f);
  std::vector<std::shared_ptr<FixedLayer>> items;
  for (int index = 0; index < 3; ++index) {
    auto item = std::make_shared<FixedLayer>(60.0f, 10.0f);
    items.push_back(item);
    row->addChild(item);
  }

  const Size measured = row->measure({ 130.0f, 100.0f });
  EXPECT_FLOAT_EQ(measured.width, 120.0f) << "the widest line, not the sum of all three";
  EXPECT_FLOAT_EQ(measured.height, 20.0f);

  row->setBounds(Rect::MakeWH(130.0f, 100.0f));
  row->layout();
  EXPECT_FLOAT_EQ(items[0]->bounds().getTop(), 0.0f);
  EXPECT_FLOAT_EQ(items[1]->bounds().getTop(), 0.0f);
  EXPECT_FLOAT_EQ(items[2]->bounds().getTop(), 10.0f);
  EXPECT_FLOAT_EQ(items[2]->bounds().getLeft(), 0.0f);
}

TEST(UiLayout, MaxSizeAndAlignSelfOverrideTheContainerRules) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Center);
  auto capped = std::make_shared<FixedLayer>(100.0f, 20.0f);
  LayoutParams cappedParams;
  cappedParams.maxWidth = 40.0f;
  capped->setLayoutParams(cappedParams);
  auto anchored = std::make_shared<FixedLayer>(20.0f, 20.0f);
  LayoutParams anchoredParams;
  anchoredParams.alignSelf = AlignSelf::End;
  anchored->setLayoutParams(anchoredParams);
  row->addChild(capped);
  row->addChild(anchored);

  row->measure({ 200.0f, 100.0f });
  row->setBounds(Rect::MakeWH(200.0f, 100.0f));
  row->layout();
  EXPECT_FLOAT_EQ(capped->bounds().getWidth(), 40.0f);
  EXPECT_FLOAT_EQ(capped->bounds().getTop(), 40.0f) << "the container centres on the cross axis";
  EXPECT_FLOAT_EQ(anchored->bounds().getTop(), 80.0f) << "align-self end wins for that child";
}

TEST(UiLayout, AnAbsoluteChildIsPositionedAgainstTheInnerRect) {
  auto parent = std::make_shared<Flex>(Axis::Row);
  parent->setPadding({ 10.0f, 10.0f, 10.0f, 10.0f });
  auto pinned = std::make_shared<FixedLayer>(30.0f, 40.0f);
  LayoutParams pinnedParams;
  pinnedParams.position = Position::Absolute;
  pinnedParams.hasRight = true;
  pinnedParams.right = 10.0f;
  pinnedParams.hasTop = true;
  pinnedParams.top = 5.0f;
  pinned->setLayoutParams(pinnedParams);

  auto stretched = std::make_shared<FixedLayer>(0.0f, 0.0f);
  LayoutParams stretchedParams;
  stretchedParams.position = Position::Absolute;
  stretchedParams.hasLeft = true;
  stretchedParams.left = 5.0f;
  stretchedParams.hasRight = true;
  stretchedParams.right = 15.0f;
  stretchedParams.hasTop = true;
  stretchedParams.top = 5.0f;
  stretchedParams.hasBottom = true;
  stretchedParams.bottom = 5.0f;
  stretched->setLayoutParams(stretchedParams);

  parent->addChild(pinned);
  parent->addChild(stretched);
  parent->measure({ 200.0f, 100.0f });
  parent->setBounds(Rect::MakeWH(200.0f, 100.0f));
  parent->layout();

  // Inner rect is (10, 10)..(190, 90).
  EXPECT_FLOAT_EQ(pinned->bounds().getLeft(), 150.0f);
  EXPECT_FLOAT_EQ(pinned->bounds().getTop(), 15.0f);
  EXPECT_FLOAT_EQ(stretched->bounds().getLeft(), 15.0f);
  EXPECT_FLOAT_EQ(stretched->bounds().getTop(), 15.0f);
  EXPECT_FLOAT_EQ(stretched->bounds().getWidth(), 160.0f);
  EXPECT_FLOAT_EQ(stretched->bounds().getHeight(), 70.0f);
}

} // namespace

TEST(UiLayout, RowGrowSplitsEqualWidths) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setBounds(Rect::MakeWH(200.0f, 60.0f));
  LayoutParams grow;
  grow.grow = 1.0f;
  auto left = std::make_shared<ColorLayer>(Color::MakeRGBA8(255, 0, 0, 255));
  auto right = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 255, 0, 255));
  left->setLayoutParams(grow);
  right->setLayoutParams(grow);
  row->addChild(left);
  row->addChild(right);
  row->measure(Size{ 200.0f, 60.0f });
  row->layout();
  EXPECT_FLOAT_EQ(left->bounds().getWidth(), 100.0f) << "grow must split the row";
  EXPECT_FLOAT_EQ(right->bounds().getLeft(), 100.0f);
  EXPECT_FLOAT_EQ(right->bounds().getWidth(), 100.0f);
}

TEST(UiLayout, RowFlexMirrorsItsChildrenInRtl) {
  auto row = std::make_shared<Flex>(Axis::Row);
  auto first = std::make_shared<FixedLayer>(10.0f, 20.0f);
  auto second = std::make_shared<FixedLayer>(20.0f, 20.0f);
  auto third = std::make_shared<FixedLayer>(30.0f, 20.0f);
  row->addChild(first);
  row->addChild(second);
  row->addChild(third);

  row->measure({ 100.0f, 40.0f });
  row->setBounds(Rect::MakeWH(100.0f, 40.0f));
  row->layout();
  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 0.0f);
  EXPECT_FLOAT_EQ(second->bounds().getLeft(), 10.0f);
  EXPECT_FLOAT_EQ(third->bounds().getLeft(), 30.0f);

  row->setDirection(Direction::Rtl);
  row->layout();
  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 90.0f) << "the first child sits at the right edge";
  EXPECT_FLOAT_EQ(second->bounds().getLeft(), 70.0f);
  EXPECT_FLOAT_EQ(third->bounds().getLeft(), 40.0f);
}

TEST(UiLayout, RtlSwapsTheHorizontalPadding) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setPadding(Insets{ 5.0f, 0.0f, 15.0f, 0.0f });
  row->setDirection(Direction::Rtl);
  auto first = std::make_shared<FixedLayer>(10.0f, 20.0f);
  auto third = std::make_shared<FixedLayer>(30.0f, 20.0f);
  row->addChild(first);
  row->addChild(third);

  row->measure({ 100.0f, 40.0f });
  row->setBounds(Rect::MakeWH(100.0f, 40.0f));
  row->layout();
  EXPECT_FLOAT_EQ(first->bounds().getRight(), 95.0f) << "start padding (5) sits on the right";
  EXPECT_FLOAT_EQ(third->bounds().getRight(), 85.0f) << "content ends 15 from the left edge";
  EXPECT_FLOAT_EQ(third->bounds().getLeft(), 55.0f);
}

TEST(UiLayout, DirectionInheritsFromTheParent) {
  auto column = std::make_shared<Flex>(Axis::Column);
  auto row = std::make_shared<Flex>(Axis::Row);
  column->addChild(row);
  EXPECT_EQ(row->direction(), Direction::Ltr);
  column->setDirection(Direction::Rtl);
  EXPECT_EQ(row->direction(), Direction::Rtl) << "an unset direction inherits the ancestor";
  row->setDirection(Direction::Ltr);
  EXPECT_EQ(row->direction(), Direction::Ltr) << "an explicit direction wins";
  EXPECT_TRUE(row->hasExplicitDirection());
}

TEST(UiLayout, TableSplitsFixedAndWeightedColumns) {
  Table table({ TableColumn{ 80.0f, 1.0f }, TableColumn{ 0.0f, 1.0f },
    TableColumn{ 0.0f, 2.0f } });
  table.setPadding(Insets{ 10.0f, 10.0f, 10.0f, 10.0f });
  table.setGap(5.0f);
  auto first = std::make_shared<FixedLayer>(80.0f, 20.0f);
  auto second = std::make_shared<FixedLayer>(10.0f, 30.0f);
  auto third = std::make_shared<FixedLayer>(10.0f, 15.0f);
  table.setRows({ { first, second, third } });
  table.setBounds(Rect::MakeWH(300.0f, 100.0f));
  table.layout();

  EXPECT_FLOAT_EQ(first->bounds().getWidth(), 80.0f) << "a fixed column keeps its width";
  EXPECT_FLOAT_EQ(second->bounds().getWidth(), 190.0f / 3.0f);
  EXPECT_FLOAT_EQ(third->bounds().getWidth(), 380.0f / 3.0f);
  EXPECT_FLOAT_EQ(second->bounds().getLeft(), 10.0f + 80.0f + 5.0f);
  EXPECT_FLOAT_EQ(first->bounds().getTop(), 10.0f);
  EXPECT_FLOAT_EQ(second->bounds().getHeight(), 30.0f) << "the row is as tall as its tallest cell";
  EXPECT_FLOAT_EQ(third->bounds().getHeight(), 30.0f);

  const Size measured = table.measure(Size{ 300.0f, 0.0f });
  EXPECT_FLOAT_EQ(measured.height, 10.0f + 10.0f + 30.0f);
}

TEST(UiLayout, TableRowsRespectHolesAndGaps) {
  Table table({ TableColumn{ 100.0f, 1.0f }, TableColumn{ 100.0f, 1.0f } });
  table.setPadding(Insets{ 0.0f, 0.0f, 0.0f, 0.0f });
  table.setGap(10.0f);
  auto a = std::make_shared<FixedLayer>(100.0f, 20.0f);
  auto b = std::make_shared<FixedLayer>(100.0f, 20.0f);
  table.setRows({ { a, nullptr }, { nullptr, b } });
  table.setBounds(Rect::MakeWH(210.0f, 100.0f));
  table.layout();

  EXPECT_FLOAT_EQ(a->bounds().getLeft(), 0.0f);
  EXPECT_FLOAT_EQ(a->bounds().getTop(), 0.0f);
  EXPECT_FLOAT_EQ(b->bounds().getLeft(), 110.0f);
  EXPECT_FLOAT_EQ(b->bounds().getTop(), 30.0f) << "the first row plus the row gap";
  EXPECT_FLOAT_EQ(table.resolveRowHeights().size(), 2u);
}

TEST(UiLayout, AspectRatioDerivesTheCrossAxis) {
  LayoutParams params;
  params.basis = 100.0f;
  params.aspectRatio = 2.0f;

  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Start);
  auto box = std::make_shared<FixedLayer>(10.0f, 10.0f);
  box->setLayoutParams(params);
  row->addChild(box);
  row->measure({ 300.0f, 300.0f });
  EXPECT_FLOAT_EQ(row->measuredSize().height, 50.0f) << "width 100 at 2:1 is 50 tall";
  row->setBounds(Rect::MakeWH(300.0f, 300.0f));
  row->layout();
  EXPECT_FLOAT_EQ(box->bounds().getWidth(), 100.0f);
  EXPECT_FLOAT_EQ(box->bounds().getHeight(), 50.0f);

  LayoutParams portrait;
  portrait.basis = 100.0f;
  portrait.aspectRatio = 0.5f;
  auto column = std::make_shared<Flex>(Axis::Column);
  column->setCrossAlign(Align::Start);
  auto card = std::make_shared<FixedLayer>(10.0f, 10.0f);
  card->setLayoutParams(portrait);
  column->addChild(card);
  column->measure({ 300.0f, 300.0f });
  column->setBounds(Rect::MakeWH(300.0f, 300.0f));
  column->layout();
  EXPECT_FLOAT_EQ(card->bounds().getHeight(), 100.0f);
  EXPECT_FLOAT_EQ(card->bounds().getWidth(), 50.0f) << "height 100 at 1:2 is 50 wide";
}

TEST(UiLayout, AspectRatioZeroKeepsTheMeasuredSize) {
  LayoutParams params;
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Start);
  auto box = std::make_shared<FixedLayer>(80.0f, 20.0f);
  box->setLayoutParams(params);
  row->addChild(box);
  row->measure({ 300.0f, 300.0f });
  EXPECT_FLOAT_EQ(row->measuredSize().height, 20.0f);
}

TEST(UiLayout, ScrollViewAnimatesToAnOffsetAndCanBeStopped) {
  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  content->setBounds(Rect::MakeWH(100.0f, 1000.0f));
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->measure({ 100.0f, 100.0f });
  view->layout();

  view->animateScrollTo(500.0f, std::chrono::milliseconds(200), Curve::linear());
  EXPECT_TRUE(view->scrollAnimating());
  EXPECT_TRUE(view->hasRunningAnimation());
  view->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(250));
  EXPECT_FALSE(view->scrollAnimating());
  EXPECT_FLOAT_EQ(view->scrollY(), 500.0f);

  view->animateScrollTo(0.0f, std::chrono::milliseconds(200), Curve::linear());
  view->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(100));
  EXPECT_GT(view->scrollY(), 0.0f);
  EXPECT_LT(view->scrollY(), 500.0f) << "mid-flight the offset is between the endpoints";

  view->stopScrollAnimation();
  const float stopped = view->scrollY();
  view->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(500));
  EXPECT_FLOAT_EQ(view->scrollY(), stopped) << "a stopped animation does not move";

  view->animateScrollTo(120.0f, std::chrono::milliseconds(0));
  EXPECT_FALSE(view->scrollAnimating());
  EXPECT_FLOAT_EQ(view->scrollY(), 120.0f) << "a zero duration snaps";
}

TEST(UiLayout, ScrollViewFlingDeceleratesAndStopsAtTheEdge) {
  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(9, 9, 9, 255));
  content->setBounds(Rect::MakeWH(100.0f, 1000.0f));
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->measure({ 100.0f, 100.0f });
  view->layout();

  view->fling(1200.0f);
  EXPECT_TRUE(view->scrollFlinging());
  EXPECT_TRUE(view->hasRunningAnimation());
  const auto start = std::chrono::steady_clock::now();
  for (int step = 1; step <= 60; ++step) {
    view->onFrame(start + std::chrono::milliseconds(16 * step));
  }
  EXPECT_GT(view->scrollY(), 150.0f) << "the fling covers real distance";
  EXPECT_LT(view->scrollY(), 900.0f);
  EXPECT_LT(view->flingVelocity(), 1200.0f) << "friction slows the fling";

  for (int step = 61; step <= 200; ++step) {
    view->onFrame(start + std::chrono::milliseconds(16 * step));
  }
  EXPECT_FALSE(view->scrollFlinging()) << "a fling eventually stops";

  view->setScrollOffset(0.0f, 900.0f);
  view->fling(1200.0f);
  view->onFrame(std::chrono::steady_clock::now());
  view->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(16));
  EXPECT_FALSE(view->scrollFlinging()) << "momentum ends when the clamp edge is reached";
}

TEST(UiLayout, ScrollEventsChainToTheParentAtAnEdge) {
  auto outer = std::make_shared<ScrollView>();
  auto outerContent = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 1, 1, 255));
  outerContent->setBounds(Rect::MakeWH(120.0f, 800.0f));
  outer->setContent(outerContent);
  outer->setBounds(Rect::MakeWH(120.0f, 200.0f));
  outer->measure({ 120.0f, 200.0f });
  outer->layout();

  auto inner = std::make_shared<ScrollView>();
  auto innerContent = std::make_shared<ColorLayer>(Color::MakeRGBA8(2, 2, 2, 255));
  innerContent->setBounds(Rect::MakeWH(100.0f, 100.0f));
  inner->setContent(innerContent);
  inner->setBounds(Rect::MakeWH(100.0f, 100.0f));
  inner->measure({ 100.0f, 100.0f });
  inner->layout();
  outer->addChild(inner);

  // The inner view has no scrollable range, so it declines and the parent takes the wheel.
  ScrollEvent wheel;
  wheel.position = { 10.0f, 10.0f };
  wheel.deltaY = -30.0f;
  EXPECT_FALSE(inner->onScroll(wheel)) << "a view at its edge does not consume the wheel";
  EXPECT_TRUE(outer->onScroll(wheel));
  EXPECT_FLOAT_EQ(outer->scrollY(), 30.0f);

  // A view that can still move consumes the event and stops chaining.
  innerContent->setBounds(Rect::MakeWH(100.0f, 400.0f));
  inner->layout();
  EXPECT_TRUE(inner->onScroll(wheel));
  EXPECT_GT(inner->scrollY(), 0.0f);
  EXPECT_FLOAT_EQ(outer->scrollY(), 30.0f) << "the parent does not also scroll";

  // Scroll to the end, then the next wheel is declined and chains upward.
  ScrollEvent up;
  up.position = { 10.0f, 10.0f };
  up.deltaY = -500.0f;
  EXPECT_TRUE(inner->onScroll(up)) << "the first large wheel reaches the end";
  EXPECT_FLOAT_EQ(inner->scrollY(), 300.0f);
  EXPECT_FALSE(inner->onScroll(up)) << "clamped at the end, the wheel chains upward";
  EXPECT_TRUE(outer->onScroll(up));
}

TEST(UiLayout, GridMirrorsColumnsInRtl) {
  auto grid = std::make_shared<Grid>(2);
  auto first = std::make_shared<FixedLayer>(40.0f, 20.0f);
  auto second = std::make_shared<FixedLayer>(40.0f, 20.0f);
  grid->addChild(first);
  grid->addChild(second);
  grid->measure({ 100.0f, 100.0f });
  grid->setBounds(Rect::MakeWH(100.0f, 100.0f));
  grid->layout();
  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 0.0f);
  EXPECT_FLOAT_EQ(second->bounds().getLeft(), 54.0f) << "46pt cell plus the 8pt gap";

  grid->setDirection(Direction::Rtl);
  grid->layout();
  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 54.0f) << "RTL mirrors the first column to the right";
  EXPECT_FLOAT_EQ(second->bounds().getLeft(), 0.0f);
}

class CountingMeasureLayer final : public Layer {
 public:
  Size measure(const Size& available) override {
    (void)available;
    ++calls;
    setMeasuredSize({ 40.0f, 20.0f });
    return measuredSize();
  }

  int calls = 0;
};

TEST(UiLayout, MeasureCacheReusesTheResultUntilLayoutChanges) {
  CountingMeasureLayer layer;
  EXPECT_FLOAT_EQ(layer.measureCached({ 100.0f, 100.0f }).width, 40.0f);
  EXPECT_FLOAT_EQ(layer.measureCached({ 100.0f, 100.0f }).width, 40.0f);
  EXPECT_EQ(layer.calls, 1) << "the same available size is measured once";

  EXPECT_FLOAT_EQ(layer.measureCached({ 50.0f, 100.0f }).width, 40.0f);
  EXPECT_EQ(layer.calls, 2) << "a different available size remeasures";

  layer.setNeedsLayout();
  layer.measureCached({ 100.0f, 100.0f });
  EXPECT_EQ(layer.calls, 3) << "setNeedsLayout invalidates the cache";
}

TEST(UiLayout, IntrinsicSizeAggregatesChildrenAndText) {
  Layer plain;
  EXPECT_EQ(plain.intrinsicSize().width, -1.0f) << "a plain layer has no intrinsic size";

  auto row = std::make_shared<Flex>(Axis::Row);
  row->setGap(4.0f);
  row->addChild(std::make_shared<FixedLayer>(30.0f, 10.0f));
  row->addChild(std::make_shared<FixedLayer>(50.0f, 20.0f));
  const Size rowIntrinsic = row->intrinsicSize();
  EXPECT_FLOAT_EQ(rowIntrinsic.width, 84.0f) << "30 + 50 + one 4pt gap";
  EXPECT_FLOAT_EQ(rowIntrinsic.height, 20.0f) << "the tallest child wins the cross axis";

  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface != nullptr) {
    const auto font = graphics::Font::create(typeface, 14.0f);
    TextLayer label("Hello", font);
    EXPECT_FLOAT_EQ(label.intrinsicSize().width, font->measureWidth("Hello"));
    EXPECT_GT(label.intrinsicSize().height, 0.0f);
  }
}

TEST(UiLayout, DockMirrorsSidePanelsInRtl) {
  DockView dock;
  dock.setBounds(Rect::MakeWH(400.0f, 300.0f));
  auto left = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 0, 0, 255));
  auto center = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 1, 0, 255));
  auto right = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 0, 1, 255));
  dock.setMinimumPaneSize(0.0f);
  dock.setPanel(DockView::Region::Left, left, 100.0f);
  dock.setPanel(DockView::Region::Center, center, 200.0f);
  dock.setPanel(DockView::Region::Right, right, 60.0f);
  dock.layout();
  EXPECT_FLOAT_EQ(left->bounds().getLeft(), 0.0f);
  EXPECT_FLOAT_EQ(left->bounds().getWidth(), 100.0f);
  EXPECT_FLOAT_EQ(right->bounds().getLeft(), 340.0f) << "the right panel sits at the right edge";

  dock.setDirection(Direction::Rtl);
  dock.layout();
  EXPECT_FLOAT_EQ(left->bounds().getLeft(), 300.0f) << "the left panel moves to the right edge";
  EXPECT_FLOAT_EQ(left->bounds().getWidth(), 100.0f) << "and keeps its width";
  EXPECT_FLOAT_EQ(right->bounds().getLeft(), 0.0f) << "the right panel moves to the left edge";
  EXPECT_FLOAT_EQ(right->bounds().getWidth(), 60.0f);
}

TEST(UiLayout, FitContentUsesTheIntrinsicSize) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto font = graphics::Font::create(typeface, 14.0f);

  LayoutParams params;
  params.fitContentMain = true;
  auto label = std::make_shared<TextLayer>("Fit", font);
  label->setLayoutParams(params);
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Start);
  row->addChild(label);
  row->measure({ 300.0f, 100.0f });
  row->setBounds(Rect::MakeWH(300.0f, 100.0f));
  row->layout();
  EXPECT_FLOAT_EQ(label->bounds().getWidth(), font->measureWidth("Fit"))
    << "fitContentMain uses the intrinsic width instead of the slot";
}

TEST(UiLayout, UnboundedMainMeasuresTheChildNaturalSize) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto font = graphics::Font::create(typeface, 14.0f);
  const char* text = "A long label that cannot fit in a narrow slot without wrapping";

  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Start);
  LayoutParams bounded;
  auto boundedLabel = std::make_shared<TextLayer>(text, font);
  boundedLabel->setLayoutParams(bounded);
  row->addChild(boundedLabel);
  row->measure({ 60.0f, 200.0f });
  row->setBounds(Rect::MakeWH(60.0f, 200.0f));
  row->layout();
  const float wrappedHeight = boundedLabel->bounds().getHeight();
  ASSERT_GT(wrappedHeight, 0.0f);

  LayoutParams unbounded;
  unbounded.unboundedMain = true;
  auto unboundedLabel = std::make_shared<TextLayer>(text, font);
  unboundedLabel->setLayoutParams(unbounded);
  auto second = std::make_shared<Flex>(Axis::Row);
  second->setCrossAlign(Align::Start);
  second->addChild(unboundedLabel);
  second->measure({ 60.0f, 200.0f });
  second->setBounds(Rect::MakeWH(60.0f, 200.0f));
  second->layout();
  EXPECT_LT(unboundedLabel->bounds().getHeight(), wrappedHeight)
    << "an unbounded main axis reports the natural single-line height";
  EXPECT_GT(unboundedLabel->bounds().getWidth(), 60.0f)
    << "and the natural width exceeds the narrow slot";
}

TEST(UiLayout, BaselineAlignsTextOfDifferentSizes) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto small = graphics::Font::create(typeface, 12.0f);
  const auto large = graphics::Font::create(typeface, 24.0f);

  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Baseline);
  auto label = std::make_shared<TextLayer>("Small", small);
  auto big = std::make_shared<TextLayer>("Big", large);
  row->addChild(label);
  row->addChild(big);
  row->measure({ 300.0f, 100.0f });
  row->setBounds(Rect::MakeWH(300.0f, 100.0f));
  row->layout();

  const float smallBaseline = label->bounds().getTop() + small->metrics().ascent;
  const float largeBaseline = big->bounds().getTop() + large->metrics().ascent;
  EXPECT_NEAR(smallBaseline, largeBaseline, 0.01f) << "the first text baselines line up";
}

TEST(UiLayout, DraggingTheScrollbarThumbScrollsTheView) {
  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(3, 3, 3, 255));
  content->setBounds(Rect::MakeWH(100.0f, 1000.0f));
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->measure({ 100.0f, 100.0f });
  view->layout();

  const Rect thumb = view->scrollbarThumbBounds();
  ASSERT_FALSE(thumb.isEmpty());
  PointerEvent down;
  down.position = { thumb.getLeft() + 1.0f, thumb.getTop() + 2.0f };
  ASSERT_TRUE(view->onPointerDown(down));
  PointerEvent move;
  move.position = { down.position.x, down.position.y + 20.0f };
  ASSERT_TRUE(view->onPointerMove(move));
  EXPECT_GT(view->scrollY(), 0.0f) << "dragging the thumb scrolls proportionally";
  const float afterDrag = view->scrollY();
  PointerEvent up;
  up.position = move.position;
  EXPECT_TRUE(view->onPointerUp(up));
  EXPECT_FALSE(view->onPointerMove(move)) << "the drag ends on release";
  EXPECT_FLOAT_EQ(view->scrollY(), afterDrag);
}

TEST(UiLayout, BounceAllowsOverscrollAndSpringsBack) {
  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(4, 4, 4, 255));
  content->setBounds(Rect::MakeWH(100.0f, 1000.0f));
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->measure({ 100.0f, 100.0f });
  view->layout();
  view->setBounceEnabled(true);
  view->setBounceLimit(60.0f);

  ScrollEvent wheel;
  wheel.position = { 10.0f, 10.0f };
  wheel.deltaY = 300.0f;
  ASSERT_TRUE(view->onScroll(wheel)) << "an overscroll at the top is still consumed";
  EXPECT_LT(view->scrollY(), 0.0f) << "the offset goes past the top edge";
  EXPECT_GE(view->scrollY(), -60.0f) << "but is limited by the bounce limit";

  view->onFrame(std::chrono::steady_clock::now());
  view->onFrame(std::chrono::steady_clock::now() + std::chrono::milliseconds(300));
  EXPECT_FLOAT_EQ(view->scrollY(), 0.0f) << "the next frames spring back to the edge";
}

TEST(UiLayout, TableViewSelectsThePressedRow) {
  auto view = std::make_shared<TableView>();
  view->setColumns({ TableColumn{ 0.0f, 1.0f }, TableColumn{ 0.0f, 1.0f } });
  view->setGap(0.0f);
  view->setPadding(Insets{});
  std::vector<std::vector<std::shared_ptr<Layer>>> rows;
  for (int row = 0; row < 3; ++row) {
    auto first = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 0, 0, 255));
    first->setBounds(Rect::MakeWH(50.0f, 20.0f));
    auto second = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 10, 0, 255));
    second->setBounds(Rect::MakeWH(50.0f, 20.0f));
    rows.push_back({ first, second });
  }
  view->setRows(rows);
  view->setBounds(Rect::MakeWH(100.0f, 60.0f));
  view->measure({ 100.0f, 60.0f });
  view->layout();

  int activated = -1;
  view->setOnRowActivated([&activated](const int row) { activated = row; });
  PointerEvent press;
  press.position = { 10.0f, 25.0f };
  ASSERT_TRUE(view->onPointerDown(press)) << "the press lands on a row";
  EXPECT_EQ(view->selectedRow(), 1);
  EXPECT_EQ(activated, 1);

  PointerEvent miss;
  miss.position = { 10.0f, 200.0f };
  EXPECT_FALSE(view->onPointerDown(miss));
  EXPECT_EQ(view->selectedRow(), 1) << "a miss keeps the selection";
}

TEST(UiLayout, TableViewKeyboardMovesTheSelectionAndRevealsIt) {
  auto view = std::make_shared<TableView>();
  view->setColumns({ TableColumn{ 0.0f, 1.0f } });
  view->setGap(0.0f);
  view->setPadding(Insets{});
  view->setBounds(Rect::MakeWH(100.0f, 60.0f));
  std::vector<std::vector<std::shared_ptr<Layer>>> rows;
  for (int row = 0; row < 30; ++row) {
    auto cell = std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 20, 20, 255));
    cell->setBounds(Rect::MakeWH(100.0f, 20.0f));
    rows.push_back({ cell });
  }
  view->setRows(rows);
  view->measure({ 100.0f, 60.0f });
  view->layout();
  ASSERT_EQ(view->selectedRow(), -1);

  KeyEvent down;
  down.key = Key::Down;
  ASSERT_TRUE(view->onKeyDown(down));
  EXPECT_EQ(view->selectedRow(), 0) << "the first Down selects the first row";

  for (int step = 0; step < 10; ++step) view->onKeyDown(down);
  EXPECT_EQ(view->selectedRow(), 10);
  EXPECT_GT(view->scrollY(), 0.0f) << "the view scrolled to reveal the row";

  KeyEvent end;
  end.key = Key::End;
  ASSERT_TRUE(view->onKeyDown(end));
  EXPECT_EQ(view->selectedRow(), 29);
  KeyEvent home;
  home.key = Key::Home;
  ASSERT_TRUE(view->onKeyDown(home));
  EXPECT_EQ(view->selectedRow(), 0);

  int activated = -1;
  view->setOnRowActivated([&activated](const int row) { activated = row; });
  KeyEvent enter;
  enter.key = Key::Enter;
  ASSERT_TRUE(view->onKeyDown(enter));
  EXPECT_EQ(activated, 0);
}

TEST(UiLayout, ScrollViewScrollsHorizontallyWithWheelDeltaX) {
  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<FixedLayer>(400.0f, 100.0f);
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->measure({ 100.0f, 100.0f });
  view->layout();

  EXPECT_FLOAT_EQ(view->contentSize().width, 400.0f);
  view->setScrollOffset(40.0f, 0.0f);
  EXPECT_FLOAT_EQ(view->scrollX(), 40.0f) << "a wide content allows a horizontal offset";
  view->setScrollOffset(0.0f, 0.0f);
  ScrollEvent wheel;
  wheel.position = { 10.0f, 10.0f };
  wheel.deltaX = 40.0f;
  ASSERT_TRUE(view->onScroll(wheel));
  EXPECT_FLOAT_EQ(view->scrollX(), 40.0f) << "deltaX scrolls the horizontal axis";
  EXPECT_FLOAT_EQ(view->scrollY(), 0.0f);
}

TEST(UiLayout, GridHonoursPercentageSizedCells) {
  auto grid = std::make_shared<Grid>();
  grid->setColumns(2);
  grid->setGap(0.0f);
  grid->setPadding(Insets{});
  auto full = std::make_shared<ColorLayer>(Color::MakeRGBA8(9, 9, 9, 255));
  auto half = std::make_shared<ColorLayer>(Color::MakeRGBA8(7, 7, 7, 255));
  LayoutParams params;
  params.widthPercent = 0.5f;
  half->setLayoutParams(params);
  grid->addChild(full);
  grid->addChild(half);
  grid->setBounds(Rect::MakeWH(200.0f, 100.0f));
  grid->measure({ 200.0f, 100.0f });
  grid->layout();

  EXPECT_FLOAT_EQ(full->bounds().getWidth(), 100.0f) << "an unset percentage fills the cell";
  EXPECT_FLOAT_EQ(half->bounds().getWidth(), 50.0f) << "50% resolves against the cell width";
}

TEST(UiLayout, HorizontalScrollbarAppearsAndDrags) {
  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<FixedLayer>(400.0f, 100.0f);
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->measure({ 100.0f, 100.0f });
  view->layout();

  const Rect thumb = view->horizontalScrollbarThumbBounds();
  ASSERT_FALSE(thumb.isEmpty()) << "wide content shows a horizontal thumb";
  EXPECT_TRUE(view->scrollbarThumbBounds().isEmpty()) << "tall content would show the vertical one";

  PointerEvent down;
  down.position = { thumb.getLeft() + 1.0f, thumb.getTop() + 1.0f };
  ASSERT_TRUE(view->onPointerDown(down));
  PointerEvent move;
  move.position = { down.position.x + 15.0f, down.position.y };
  ASSERT_TRUE(view->onPointerMove(move));
  EXPECT_GT(view->scrollX(), 0.0f) << "dragging the horizontal thumb scrolls the X axis";
  EXPECT_FLOAT_EQ(view->scrollY(), 0.0f);
  PointerEvent up;
  up.position = move.position;
  EXPECT_TRUE(view->onPointerUp(up));
}

TEST(UiLayout, TableExposesRowBoundsAndSelection) {
  Table table;
  table.setColumns({ TableColumn{ 0.0f, 1.0f } });
  table.setGap(4.0f);
  table.setPadding(Insets{ 5.0f, 5.0f, 5.0f, 5.0f });
  std::vector<std::vector<std::shared_ptr<Layer>>> rows;
  for (int row = 0; row < 3; ++row) {
    auto cell = std::make_shared<ColorLayer>(Color::MakeRGBA8(30, 30, 30, 255));
    cell->setBounds(Rect::MakeWH(90.0f, 20.0f));
    rows.push_back({ cell });
  }
  table.setRows(rows);
  table.setBounds(Rect::MakeWH(100.0f, 200.0f));
  table.measure({ 100.0f, 200.0f });
  table.layout();

  EXPECT_FLOAT_EQ(table.rowBounds(1).getTop(), 5.0f + 20.0f + 4.0f);
  EXPECT_FLOAT_EQ(table.rowBounds(2).getTop(), 5.0f + (20.0f + 4.0f) * 2.0f);
  EXPECT_TRUE(table.rowBounds(3).isEmpty()) << "out-of-range rows have no bounds";

  EXPECT_EQ(table.selectedRow(), -1);
  table.setSelectedRow(1);
  EXPECT_EQ(table.selectedRow(), 1);
  table.setSelectedRow(9);
  EXPECT_EQ(table.selectedRow(), -1) << "an invalid selection clears it";
}

TEST(UiLayout, TableHoverTracksTheRowUnderThePointer) {
  Table table;
  table.setColumns({ TableColumn{ 0.0f, 1.0f } });
  table.setGap(0.0f);
  table.setPadding(Insets{});
  std::vector<std::vector<std::shared_ptr<Layer>>> rows;
  for (int row = 0; row < 3; ++row) {
    auto cell = std::make_shared<ColorLayer>(Color::MakeRGBA8(30, 30, 30, 255));
    cell->setBounds(Rect::MakeWH(90.0f, 20.0f));
    rows.push_back({ cell });
  }
  table.setRows(rows);
  table.setBounds(Rect::MakeWH(100.0f, 100.0f));
  table.measure({ 100.0f, 100.0f });
  table.layout();

  EXPECT_EQ(table.rowAtLocal({ 5.0f, 25.0f }), 1) << "the point lands in the second row";
  EXPECT_EQ(table.rowAtLocal({ 5.0f, 90.0f }), -1) << "below the rows there is no row";
  EXPECT_EQ(table.rowAtLocal({ 200.0f, 25.0f }), -1) << "outside the horizontal bounds";

  table.setHoveredRow(1);
  EXPECT_EQ(table.hoveredRow(), 1);
  table.setHoveredRow(7);
  EXPECT_EQ(table.hoveredRow(), -1) << "an invalid hover clears it";
}

TEST(UiLayout, TableHonoursPercentageColumns) {
  Table table;
  table.setColumns({ TableColumn{ 0.0f, 1.0f, 0.25f }, TableColumn{ 0.0f, 1.0f } });
  table.setGap(0.0f);
  table.setPadding(Insets{});
  auto first = std::make_shared<ColorLayer>(Color::MakeRGBA8(9, 9, 9, 255));
  first->setBounds(Rect::MakeWH(10.0f, 20.0f));
  auto second = std::make_shared<ColorLayer>(Color::MakeRGBA8(7, 7, 7, 255));
  second->setBounds(Rect::MakeWH(10.0f, 20.0f));
  table.setRows({ { first, second } });
  table.setBounds(Rect::MakeWH(200.0f, 100.0f));
  table.measure({ 200.0f, 100.0f });
  table.layout();

  EXPECT_FLOAT_EQ(first->bounds().getWidth(), 50.0f) << "25% of the 200pt content width";
  EXPECT_FLOAT_EQ(second->bounds().getWidth(), 150.0f) << "the weight column takes the rest";
}

TEST(UiLayout, TableViewMirrorsFocusOntoTheTable) {
  auto view = std::make_shared<TableView>();
  view->setColumns({ TableColumn{ 0.0f, 1.0f } });
  view->setBounds(Rect::MakeWH(100.0f, 60.0f));
  auto cell = std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 20, 20, 255));
  cell->setBounds(Rect::MakeWH(100.0f, 20.0f));
  view->setRows({ { cell } });
  view->measure({ 100.0f, 60.0f });
  view->layout();

  EXPECT_FALSE(view->table()->focused());
  view->onFocusChanged(true);
  EXPECT_TRUE(view->table()->focused()) << "keyboard focus shows the table outline";
  view->onFocusChanged(false);
  EXPECT_FALSE(view->table()->focused());
}

TEST(UiLayout, BothScrollbarsLeaveTheCornerFree) {
  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<FixedLayer>(400.0f, 1000.0f);
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 100.0f));
  view->measure({ 100.0f, 100.0f });
  view->layout();

  const Rect vertical = view->scrollbarThumbBounds();
  const Rect horizontal = view->horizontalScrollbarThumbBounds();
  ASSERT_FALSE(vertical.isEmpty());
  ASSERT_FALSE(horizontal.isEmpty());
  EXPECT_TRUE(vertical.getBottom() <= horizontal.getTop() || vertical.getLeft() >= horizontal.getRight())
    << "the two thumbs must not overlap in the corner";
  EXPECT_LE(vertical.getBottom(), horizontal.getTop() + 0.01f);
  EXPECT_LE(horizontal.getRight(), vertical.getLeft() + 0.01f);
}
TEST(UiLayout, TableExposesAnAccessibilityRole) {
  Table table;
  EXPECT_EQ(table.accessibleRole(), AccessibilityRole::Table);
  table.setAccessibleName("Properties");

  const coral::ui::AccessibilityNode node = coral::ui::accessibilityTree(table);
  EXPECT_EQ(node.role, AccessibilityRole::Table);
  EXPECT_EQ(node.name, "Properties");
}

TEST(UiLayout, TableNamesUnnamedCellsForAccessibility) {
  Table table;
  table.setColumns({ TableColumn{ 0.0f, 1.0f }, TableColumn{ 0.0f, 1.0f } });
  table.setGap(0.0f);
  table.setPadding(Insets{});
  auto first = std::make_shared<ColorLayer>(Color::MakeRGBA8(9, 9, 9, 255));
  first->setBounds(Rect::MakeWH(10.0f, 20.0f));
  auto second = std::make_shared<ColorLayer>(Color::MakeRGBA8(7, 7, 7, 255));
  second->setBounds(Rect::MakeWH(10.0f, 20.0f));
  auto named = std::make_shared<ColorLayer>(Color::MakeRGBA8(5, 5, 5, 255));
  named->setBounds(Rect::MakeWH(10.0f, 20.0f));
  named->setAccessibleName("Explicit");
  table.setRows({ { first, second }, { named } });
  table.setBounds(Rect::MakeWH(200.0f, 100.0f));
  table.measure({ 200.0f, 100.0f });
  table.layout();

  EXPECT_EQ(first->accessibleName(), "Row 1, column 1");
  EXPECT_EQ(second->accessibleName(), "Row 1, column 2");
  EXPECT_EQ(named->accessibleName(), "Explicit") << "an explicit name is not overwritten";
}

TEST(UiLayout, TextFallbackMeasuresItsRunsConsistently) {
  const std::string dir = std::string(CORAL_ASSETS_DIR) + "/fonts/";
  const auto emojiTypeface = graphics::Typeface::fromFile(dir + "NotoColorEmoji.ttf");
  const auto cjkTypeface = graphics::Typeface::fromFile(dir + "NotoSansSC-Regular.otf");
  if (emojiTypeface == nullptr || cjkTypeface == nullptr) {
    GTEST_SKIP() << "the fallback test fonts are not available";
  }
  const auto primary = graphics::Font::create(emojiTypeface, 14.0f);
  const auto cjk = graphics::Font::create(cjkTypeface, 14.0f);
  auto fallback = std::make_shared<graphics::FontFallback>(primary,
    std::vector<std::shared_ptr<const graphics::Font>>{ cjk });
  const std::string text = "Hi \xE4\xBD\xA0\xE5\xA5\xBD";

  TextLayer label(text, primary);
  label.setFontFallback(fallback);
  const Size intrinsic = label.intrinsicSize();
  EXPECT_FLOAT_EQ(intrinsic.width, fallback->measure(text))
    << "the intrinsic width is the sum of the per-face runs";
  EXPECT_FLOAT_EQ(intrinsic.height, primary->metrics().ascent + primary->metrics().descent);

  label.measure({ 0.0f, 0.0f });
  EXPECT_FLOAT_EQ(label.measuredSize().width, intrinsic.width)
    << "an unbounded measure agrees with the intrinsic width";
}

TEST(UiLayout, BaselineMeasureReservesBothAscentAndDescent) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto small = graphics::Font::create(typeface, 12.0f);
  const auto large = graphics::Font::create(typeface, 24.0f);

  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Baseline);
  auto label = std::make_shared<TextLayer>("Small", small);
  auto big = std::make_shared<TextLayer>("Big", large);
  row->addChild(label);
  row->addChild(big);
  const Size measured = row->measure({ 300.0f, 400.0f });

  const float expected = std::max(small->metrics().ascent, large->metrics().ascent) +
    std::max(small->metrics().descent, large->metrics().descent);
  EXPECT_FLOAT_EQ(measured.height, expected)
    << "baseline alignment needs the tallest baseline plus the deepest descender, not the tallest box";
  row->setBounds(Rect::MakeWH(300.0f, measured.height));
  row->layout();
  const float lowEdge = big->bounds().getTop() + large->metrics().descent;
  EXPECT_LE(lowEdge, row->bounds().getBottom() + 0.01f) << "the descender fits inside the measured row";
}

TEST(UiLayout, WrappedBaselineRowsReserveEachLinesDescent) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto font = graphics::Font::create(typeface, 16.0f);

  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Baseline);
  row->setWrap(true);
  auto one = std::make_shared<TextLayer>("one", font);
  auto two = std::make_shared<TextLayer>("two", font);
  auto three = std::make_shared<TextLayer>("three", font);
  row->addChild(one);
  row->addChild(two);
  row->addChild(three);
  const Size measured = row->measure({ 50.0f, 200.0f });

  const float ascent = font->metrics().ascent;
  const float descent = font->metrics().descent;
  const float single = std::max(font->measureWidth("one"), 1.0f);
  ASSERT_GT(measured.height, ascent + descent) << "the row wrapped into at least two lines";
  // The 50pt width fits one word per line, so all three reserve their own ascent plus descent.
  EXPECT_NEAR(measured.height / (ascent + descent), 3.0f, 0.6f)
    << "each wrapped line reserves its own ascent plus descent";
  EXPECT_GT(single, 0.0f);
}

TEST(UiLayout, BaseDirectionDoesNotChangeMeasuredAdvance) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto font = graphics::Font::create(typeface, 16.0f);
  // Mixed Arabic + Latin: the base direction mirrors the visual order, not the total advance.
  const std::string text = "abc \xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9 def";

  TextLayer ltr(text, font);
  ltr.setDirection(TextDirectionMode::Ltr);
  TextLayer rtl(text, font);
  rtl.setDirection(TextDirectionMode::Rtl);

  const Size ltrSize = ltr.measure({ 0.0f, 0.0f });
  const Size rtlSize = rtl.measure({ 0.0f, 0.0f });
  EXPECT_GT(ltrSize.width, 0.0f);
  EXPECT_FLOAT_EQ(rtlSize.width, ltrSize.width)
    << "the shaped advance sum is independent of the paragraph base direction";
  EXPECT_FLOAT_EQ(rtlSize.height, ltrSize.height);
  EXPECT_FLOAT_EQ(rtl.intrinsicSize().width, ltr.intrinsicSize().width);
  EXPECT_EQ(rtl.direction(), TextDirectionMode::Rtl);
}
TEST(UiLayout, BoundsInWindowIncludesAncestorTranslations) {
  Layer root;
  root.setBounds(Rect::MakeWH(200.0f, 200.0f));
  root.setTranslation({ 20.0f, 30.0f });
  auto nested = std::make_shared<Layer>();
  nested->setBounds(Rect::MakeXYWH(5.0f, 6.0f, 40.0f, 20.0f));
  root.addChild(nested);
  const Rect inWindow = nested->boundsInWindow();
  EXPECT_FLOAT_EQ(inWindow.getLeft(), 25.0f);
  EXPECT_FLOAT_EQ(inWindow.getTop(), 36.0f);
  EXPECT_FLOAT_EQ(inWindow.getWidth(), 40.0f);
}

TEST(UiLayout, AScrollbarTrackHasAWideEnoughHitTarget) {
  auto scroll = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 200, 0, 255));
  content->setBounds(Rect::MakeWH(100.0f, 400.0f));
  scroll->setContent(content);
  scroll->setBounds(Rect::MakeWH(100.0f, 100.0f));
  scroll->layout();
  ASSERT_FALSE(scroll->scrollbarThumbBounds().isEmpty());

  PointerEvent down;
  down.position = { 96.0f, 90.0f };
  EXPECT_TRUE(scroll->onPointerDown(down)) << "the 12px gutter accepts a track click";
  EXPECT_GT(scroll->scrollY(), 0.0f);
  PointerEvent move;
  move.position = { 96.0f, 50.0f };
  EXPECT_TRUE(scroll->onPointerMove(move));
  EXPECT_GE(scroll->scrollY(), 0.0f);
  EXPECT_TRUE(scroll->onPointerUp(move));
}

TEST(UiLayout, ARightToLeftScrollViewPutsTheBarsOnTheReadingStartSide) {
  auto scroll = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 200, 0, 255));
  content->setBounds(Rect::MakeWH(400.0f, 400.0f));
  scroll->setContent(content);
  scroll->setBounds(Rect::MakeWH(64.0f, 64.0f));
  scroll->measure({ 64.0f, 64.0f });
  scroll->layout();

  ASSERT_FALSE(scroll->scrollbarThumbBounds().isEmpty());
  const Rect ltrHorizontal = scroll->horizontalScrollbarThumbBounds();
  EXPECT_GT(scroll->scrollbarThumbBounds().getLeft(), 32.0f) << "LTR keeps the vertical bar on the right";
  EXPECT_NEAR(ltrHorizontal.getLeft(), 0.0f, 0.01f) << "the horizontal thumb starts at the left edge";
  const float thumbWidth = ltrHorizontal.getWidth();

  scroll->setDirection(Direction::Rtl);
  const Rect vertical = scroll->scrollbarThumbBounds();
  const Rect horizontal = scroll->horizontalScrollbarThumbBounds();
  ASSERT_FALSE(vertical.isEmpty());
  ASSERT_FALSE(horizontal.isEmpty());
  EXPECT_LT(vertical.getRight(), 32.0f) << "RTL moves the vertical bar to the left edge";
  EXPECT_EQ(vertical.getWidth(), 4.0f);
  EXPECT_NEAR(horizontal.getRight(), 64.0f, 0.01f)
    << "the horizontal thumb anchors at the right edge in RTL";
  EXPECT_NEAR(horizontal.getLeft(), 64.0f - thumbWidth, 0.01f);
}
TEST(UiLayout, ARightToLeftScrollViewScrollsFromTheRightEdge) {
  auto scroll = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 200, 0, 255));
  content->setBounds(Rect::MakeWH(400.0f, 400.0f));
  scroll->setContent(content);
  scroll->setBounds(Rect::MakeWH(64.0f, 64.0f));
  scroll->measure({ 64.0f, 64.0f });
  scroll->layout();
  scroll->setDirection(Direction::Rtl);
  scroll->setScrollOffset(100.0f, 0.0f);
  scroll->layout();

  // At offset zero the content's right edge meets the viewport's right edge; scrolling 100 moves it
  // right by 100, which is 64 - 400 + 100 for its left edge.
  EXPECT_FLOAT_EQ(content->bounds().getLeft(), -236.0f);

  scroll->setDirection(Direction::Ltr);
  scroll->layout();
  EXPECT_FLOAT_EQ(content->bounds().getLeft(), -100.0f)
    << "LTR keeps measuring the offset from the left edge";
}

TEST(UiLayout, ARightToLeftScrollViewInvertsTheHorizontalWheel) {
  auto scroll = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 200, 0, 255));
  content->setBounds(Rect::MakeWH(400.0f, 400.0f));
  scroll->setContent(content);
  scroll->setBounds(Rect::MakeWH(64.0f, 64.0f));
  scroll->measure({ 64.0f, 64.0f });
  scroll->layout();

  ScrollEvent event;
  event.position = { 10.0f, 10.0f };
  event.deltaX = 10.0f;

  scroll->setDirection(Direction::Rtl);
  scroll->setScrollOffset(100.0f, 0.0f);
  ASSERT_TRUE(scroll->onScroll(event));
  EXPECT_FLOAT_EQ(scroll->scrollX(), 90.0f)
    << "a rightward wheel reveals the earlier part of an RTL document";

  scroll->setDirection(Direction::Ltr);
  scroll->setScrollOffset(100.0f, 0.0f);
  ASSERT_TRUE(scroll->onScroll(event));
  EXPECT_FLOAT_EQ(scroll->scrollX(), 110.0f);
}