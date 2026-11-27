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

// Invariants of the retained-mode migration (docs/GUI保留模式架构演进.md §7 R0/R1).
//
// These assert the invalidation model directly rather than through pixels: a write that does not
// change a value must not dirty layout, a change that does affect the recorded command stream must
// dirty paint, and a keyed move must keep the subtree attached to its window. They give the next
// stage (repaint boundaries) something to tighten instead of re-deriving.

#include <ui/ColorLayer.h>
#include <ui/Flex.h>
#include <ui/Grid.h>
#include <ui/Element.h>
#include <ui/DockView.h>
#include <ui/Button.h>
#include <ui/CheckBox.h>
#include <cstdio>
#include <functional>
#include <gpu/DrawList.h>
#include <ui/Accessibility.h>
#include <ui/AccessibilityAtSpi.h>
#include <ui/Translation.h>
#include <ui/FramePlan.h>
#include <ui/CanvasBackend.h>
#include <ui/RenderContext.h>
#include <graphics/Canvas.h>
#include <ui/Layer.h>
#include <ui/Menu.h>
#include <ui/DockView.h>
#include <ui/Grid.h>
#include <ui/ScrollView.h>
#include <ui/Slider.h>
#include <ui/Style.h>
#include <ui/Table.h>
#include <ui/Reconciler.h>
#include <ui/DockView.h>
#include <ui/Grid.h>
#include <ui/ScrollView.h>
#include <graphics/Bitmap.h>
#include <cstdio>
#include <functional>
#include <gpu/DrawList.h>
#include <ui/Accessibility.h>
#include <ui/AccessibilityAtSpi.h>
#include <ui/Translation.h>
#include <ui/TextInput.h>
#include <graphics/Typeface.h>
#include <ui/TextLayer.h>
#include <ui/Window.h>

#include <gtest/gtest.h>

#include "TestSupport.h"

using namespace coral::ui;

TEST(UiRetained, WritingIdenticalLayoutParamsDoesNotDirtyLayout) {
  auto flex = std::make_shared<Flex>(Axis::Row);
  flex->setPadding({ 8, 8, 8, 8 });
  flex->addChild(std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 20, 30, 255)));
  flex->measure({ 100, 100 });
  flex->layout();
  ASSERT_FALSE(flex->needsLayout());

  LayoutParams params;
  params.grow = 1.0f;
  params.basis = 0.0f;
  flex->setLayoutParams(params);
  EXPECT_TRUE(flex->needsLayout()) << "a real change must dirty layout";

  flex->measure({ 100, 100 });
  flex->layout();
  ASSERT_FALSE(flex->needsLayout());

  flex->setLayoutParams(params);
  EXPECT_FALSE(flex->needsLayout())
    << "writing the same LayoutParams twice must not request another layout pass";
}

TEST(UiRetained, WritingIdenticalPaddingDoesNotDirtyLayout) {
  Flex flex(Axis::Column);
  flex.setPadding({ 4, 5, 6, 7 });
  flex.measure({ 50, 50 });
  flex.layout();
  ASSERT_FALSE(flex.needsLayout());

  flex.setPadding({ 4, 5, 6, 7 });
  EXPECT_FALSE(flex.needsLayout());

  flex.setPadding({ 4, 5, 6, 8 });
  EXPECT_TRUE(flex.needsLayout());
}

TEST(UiRetained, WritingIdenticalGridPaddingDoesNotDirtyLayout) {
  Grid grid(2);
  grid.setPadding({ 1, 2, 3, 4 });
  grid.measure({ 40, 40 });
  grid.layout();
  ASSERT_FALSE(grid.needsLayout());

  grid.setPadding({ 1, 2, 3, 4 });
  EXPECT_FALSE(grid.needsLayout());
}

TEST(UiRetained, TogglingClipRequestsARepaint) {
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto child = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  root->addChild(child);
  window->setRoot(root);

  ASSERT_TRUE(window->renderFrame());
  ASSERT_FALSE(root->subtreeNeedsPaint()) << "a presented frame clears the paint dirty flags";
  ASSERT_FALSE(child->subtreeNeedsPaint());

  child->setClip(true);
  EXPECT_TRUE(child->needsPaint()) << "clip changes the recorded command stream";
  EXPECT_TRUE(root->subtreeNeedsPaint()) << "a child's repaint must bubble to the root's subtree flag";
}

TEST(UiRetained, ReinsertingAChildRestoresItsWindow) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto child = std::make_shared<ColorLayer>(Color::MakeRGBA8(9, 9, 9, 255));
  root->addChild(child);
  window->setRoot(root);
  ASSERT_EQ(child->window(), window.get());

  root->removeChild(child);
  EXPECT_EQ(child->window(), nullptr);

  root->insertChild(0, child);
  EXPECT_EQ(child->window(), window.get())
    << "a keyed reorder detaches and re-attaches a layer; the window back-pointer must come back";
}

TEST(UiRetained, FrameStatisticsReportWhatTheFrameActuallyRedid) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  root->setCrossAlign(Align::Stretch);
  auto first = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 20, 30, 255));
  auto second = std::make_shared<ColorLayer>(Color::MakeRGBA8(40, 50, 60, 255));
  // Give each colour its own half of the column, otherwise both measure to zero and a colour change
  // would have no damage rectangle to report.
  LayoutParams half;
  half.grow = 1.0f;
  half.basis = 0.0f;
  first->setLayoutParams(half);
  second->setLayoutParams(half);
  root->addChild(first);
  root->addChild(second);
  window->setRoot(root);

  ASSERT_TRUE(window->renderFrame());
  // `setRoot` already ran the layout pass, so the first frame is a paint-only frame.
  FrameStatistics stats = window->frameStatistics();
  EXPECT_GE(stats.dirtyNodes, 2u) << "both colours are painted for the first time";
  EXPECT_EQ(stats.layoutDirtyNodes, 0u) << "setRoot laid the tree out before the frame";

  // Nothing changed: a retained replay. Counting happens before layout, and the previous frame
  // cleared every flag, so the counters must be zero.
  ASSERT_TRUE(window->renderFrame());
  stats = window->frameStatistics();
  EXPECT_EQ(stats.dirtyNodes, 0u) << "a clean tree must not report dirty nodes";
  EXPECT_EQ(stats.layoutDirtyNodes, 0u);

  // Changing one colour repaints exactly one node and needs no layout at all. This is the property the
  // migration is about, so it is asserted rather than inferred from timings.
  first->setColor(Color::MakeRGBA8(90, 80, 70, 255));
  ASSERT_TRUE(window->renderFrame());
  stats = window->frameStatistics();
  EXPECT_EQ(stats.dirtyNodes, 1u) << "one colour change must dirty one node";
  EXPECT_EQ(stats.layoutDirtyNodes, 0u) << "a colour change must not require a layout pass";
  EXPECT_GT(stats.damagedAreaRatio, 0.0) << "the frame should repaint a damage rectangle";
  EXPECT_LT(stats.damagedAreaRatio, 1.0) << "and not the whole window";

  // A text change touches layout as well.
  auto label = std::make_shared<TextLayer>("hello", nullptr);
  root->addChild(label);
  ASSERT_TRUE(window->renderFrame());
  stats = window->frameStatistics();
  EXPECT_GE(stats.layoutDirtyNodes, 1u) << "adding a child must request layout";
}

TEST(UiRetained, ACachedContainerPaintsItsChildrenInZOrder) {
  // Two full-size colours in a small, cacheable container. `top` is added first but carries the higher
  // z-index, so it must be painted last; the cached recording used to append children in insertion
  // order and silently drew `bottom` over it.
  const auto build = [](Window& window) {
    auto root = std::make_shared<Layer>();
    root->setBounds(Rect::MakeWH(32, 32));
    auto top = std::make_shared<ColorLayer>(Color::MakeRGBA8(220, 10, 10, 255));
    top->setZIndex(1);
    top->setBounds(Rect::MakeWH(32, 32));
    auto bottom = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 10, 220, 255));
    bottom->setBounds(Rect::MakeWH(32, 32));
    root->addChild(top);
    root->addChild(bottom);
    window.setRoot(root);
  };

  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;

  const float previousLimit = Layer::recordingCacheAreaLimit();
  ASSERT_GT(previousLimit, 0.0f) << "the test assumes the default area limit keeps the container cached";

  uint32_t cachedPixel = 0;
  {
    auto window = Window::create(options);
    ASSERT_NE(window, nullptr);
    build(*window);
    ASSERT_TRUE(window->renderFrame());
    ASSERT_TRUE(window->renderFrame()) << "the second frame replays the recording";
    graphics::Bitmap bitmap;
    ASSERT_TRUE(window->snapshot(&bitmap));
    cachedPixel = bitmap.makePixmap().getPixelARGB32(16, 16);
  }

  uint32_t uncachedPixel = 0;
  Layer::setRecordingCacheAreaLimit(-1.0f);
  {
    auto window = Window::create(options);
    ASSERT_NE(window, nullptr);
    build(*window);
    ASSERT_TRUE(window->renderFrame());
    graphics::Bitmap bitmap;
    ASSERT_TRUE(window->snapshot(&bitmap));
    uncachedPixel = bitmap.makePixmap().getPixelARGB32(16, 16);
  }
  Layer::setRecordingCacheAreaLimit(previousLimit);

  EXPECT_EQ(cachedPixel, uncachedPixel)
    << "a cached container must composite its children in the same order as the uncached path";
  EXPECT_GT((cachedPixel >> 16) & 0xFFu, 150u) << "the higher z-index colour wins";
  EXPECT_LT((cachedPixel >> 0) & 0xFFu, 80u);
}

TEST(UiRetained, RemovingAFocusedSubtreeClearsTheWindowFocus) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Row);
  auto field = std::make_shared<TextInput>("abc", nullptr);
  panel->addChild(field);
  root->addChild(panel);
  window->setRoot(root);

  window->focusLayer(field.get());
  ASSERT_EQ(window->focusedLayer(), field.get());

  // The reconciler detaches the old subtree before mounting the new one; leaving a bare pointer at the
  // removed field would make the next key event or IME query dereference freed memory.
  root->removeChild(panel);
  EXPECT_EQ(window->focusedLayer(), nullptr)
    << "removing the focused subtree must clear focus instead of keeping a dangling pointer";
}

TEST(UiRetained, ReplacingTheRootKeepsTheOverlayHostAttached) {
  Window::Options options;
  options.headless = true;
  options.width = 48;
  options.height = 36;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto first = std::make_shared<Flex>(Axis::Column);
  window->setRoot(first);
  auto panel = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  ASSERT_NE(window->pushOverlay(panel), 0u);
  ASSERT_EQ(first->children().size(), 1u) << "the overlay host is a child of the page root";

  // Swapping the page must not orphan the host: overlays pushed through `pushOverlay` have to keep
  // being drawn, and the next push must not land in a tree nobody paints.
  auto second = std::make_shared<Flex>(Axis::Row);
  window->setRoot(second);
  EXPECT_EQ(second->children().size(), 1u) << "the overlay host is re-attached to the new root";
  ASSERT_TRUE(window->renderFrame());
  EXPECT_TRUE(window->hasOverlay());
}

TEST(UiRetained, AccessibilityEventsReachTheOwningWindow) {
  Window::Options options;
  options.headless = true;
  options.width = 48;
  options.height = 36;
  auto first = Window::create(options);
  auto second = Window::create(options);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  auto firstField = std::make_shared<TextInput>("a", nullptr);
  auto secondField = std::make_shared<TextInput>("b", nullptr);
  auto firstRoot = std::make_shared<Flex>(Axis::Column);
  auto secondRoot = std::make_shared<Flex>(Axis::Column);
  firstRoot->addChild(firstField);
  secondRoot->addChild(secondField);
  first->setRoot(firstRoot);
  second->setRoot(secondRoot);

  std::vector<AccessibilityEvent> firstEvents;
  std::vector<AccessibilityEvent> secondEvents;
  setAccessibilityNotifier(first.get(), [&firstEvents](Layer&, const AccessibilityEvent e) {
    firstEvents.push_back(e);
  });
  setAccessibilityNotifier(second.get(), [&secondEvents](Layer&, const AccessibilityEvent e) {
    secondEvents.push_back(e);
  });

  firstField->setText("aa");
  EXPECT_EQ(firstEvents.size(), 1u) << "the owning window is the one that reports the change";
  EXPECT_EQ(secondEvents.size(), 0u) << "a second window on the same thread must not receive it";

  secondField->setText("bb");
  EXPECT_EQ(firstEvents.size(), 1u);
  EXPECT_EQ(secondEvents.size(), 1u);

  setAccessibilityNotifier(first.get(), {});
  setAccessibilityNotifier(second.get(), {});
  firstField->setText("aaa");
  EXPECT_EQ(firstEvents.size(), 1u) << "a removed notifier receives nothing";
}

TEST(UiRetained, OverridingDrawStillClearsThePaintFlags) {
  // ScrollView overrides `draw` instead of `drawSelf`, so Layer::draw never clears its flags for it. A
  // layer that stays dirty forever keeps its ancestors' retained short-circuit from engaging.
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto scroll = std::make_shared<ScrollView>();
  scroll->setPreferredHeight(40.0f);
  auto content = std::make_shared<Flex>(Axis::Column);
  auto band = std::make_shared<ColorLayer>(Color::MakeRGBA8(5, 6, 7, 255));
  // Taller than the viewport, so the scroll below is a real move rather than a clamped no-op.
  LayoutParams bandParams;
  bandParams.basis = 200.0f;
  band->setLayoutParams(bandParams);
  content->addChild(band);
  scroll->setContent(content);
  root->addChild(scroll);
  window->setRoot(root);

  ASSERT_TRUE(window->renderFrame());
  EXPECT_FALSE(scroll->needsPaint()) << "a drawn override must clear its own paint flag";
  EXPECT_FALSE(scroll->subtreeNeedsPaint());
  EXPECT_FALSE(root->subtreeNeedsPaint()) << "a clean subtree must not keep the root dirty";

  // Scrolling still has to dirty it, otherwise the viewport would stop updating.
  scroll->setScrollOffset(0.0f, 8.0f);
  EXPECT_TRUE(scroll->needsPaint());
  EXPECT_TRUE(root->subtreeNeedsPaint());
}

TEST(UiRetained, AnEquivalentRebuildAppliesNoProps) {
  Reconciler reconciler;
  const auto build = [](const std::string& label, Color colour) {
    std::vector<ElementPtr> children{
      std::make_shared<TextElement>(label, nullptr, colour, "label"),
      std::make_shared<ColorElement>(Color::MakeRGBA8(9, 9, 9, 255), "dot"),
    };
    return components::WithProps(
      std::make_shared<StackElement>(Axis::Row, std::move(children), "row"), ElementProps{}, "panel");
  };

  auto root = reconciler.mount(build("hello", Color::White()));
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(reconciler.lastAppliedNodeCount(), 0u) << "the counter reports update() work, not the mount";

  // Same values, fresh element objects: nothing may be re-applied.
  reconciler.update(build("hello", Color::White()));
  EXPECT_EQ(reconciler.lastAppliedNodeCount(), 0u)
    << "an equivalent rebuild must not walk the setter chain at all";

  // One changed value: exactly that node.
  reconciler.update(build("hello!", Color::White()));
  EXPECT_EQ(reconciler.lastAppliedNodeCount(), 1u);

  // A layout-only change is carried by the props wrapper.
  ElementProps props;
  props.layout.grow = 1.0f;
  std::vector<ElementPtr> children{
    std::make_shared<TextElement>("hello!", nullptr, Color::White(), "label"),
    std::make_shared<ColorElement>(Color::MakeRGBA8(9, 9, 9, 255), "dot"),
  };
  auto wrapped = components::WithProps(
    std::make_shared<StackElement>(Axis::Row, std::move(children), "row"), props, "panel");
  reconciler.update(wrapped);
  EXPECT_EQ(reconciler.lastAppliedNodeCount(), 1u)
    << "only the props wrapper changed, so only it is applied";
}

namespace {

/** Counts how often a layout container asks this child for its intrinsic size. */
class CountingIntrinsicLayer final : public Layer {
 public:
  [[nodiscard]] Size intrinsicSize() const override {
    ++mIntrinsicCalls;
    return Size{ 10.0f, 10.0f };
  }

  mutable int mIntrinsicCalls = 0;
};

} // namespace

TEST(UiRetained, LayoutDoesNotAskForIntrinsicSizeUnlessFitContent) {
  // `intrinsicSize()` on a text layer shapes the text, so asking every child for it on every measure
  // and layout made a full relayout of a 1480-node page cost tens of milliseconds for a value almost
  // no child uses.
  auto flex = std::make_shared<Flex>(Axis::Row);
  auto child = std::make_shared<CountingIntrinsicLayer>();
  flex->addChild(child);

  flex->measure({ 100, 100 });
  flex->layout();
  EXPECT_EQ(child->mIntrinsicCalls, 0) << "no fit-content hint, so nothing may ask for it";

  LayoutParams params;
  params.fitContentMain = true;
  child->setLayoutParams(params);
  flex->measure({ 100, 100 });
  flex->layout();
  EXPECT_GE(child->mIntrinsicCalls, 1) << "fit-content asks for it exactly when it is needed";
}

TEST(UiRetained, ARepaintBoundaryOwnsItsRecording) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Row);
  panel->setRepaintBoundary(true);
  auto colour = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 200, 10, 255));
  colour->setBounds(Rect::MakeWH(64, 48));
  panel->addChild(colour);
  root->addChild(panel);
  window->setRoot(root);

  ASSERT_TRUE(window->renderFrame());

  ASSERT_TRUE(window->renderFrame()) << "the second frame replays";

  EXPECT_TRUE(panel->hasCachedRecording()) << "the boundary keeps its own list";
  // Giving up the ancestor cache must not cost the zero-work frame: with nothing dirty the surface
  // replays its retained recording, so the frame records nothing even though the root holds no list.
  EXPECT_EQ(window->lastFrameTimings().draw, 0.0)
    << "a clean frame with a boundary is still a no-work frame";
  EXPECT_EQ(window->frameStatistics().dirtyNodes, 0u);
  EXPECT_FALSE(root->hasCachedRecording())
    << "the ancestor must not flatten a boundary into its own recording";


  const FrameStatistics replay = window->frameStatistics();
  EXPECT_EQ(replay.repaintedBoundaries, 0u)
    << "the second frame replayed the boundary instead of rebuilding it";
  EXPECT_EQ(replay.frameListBytes, 0u) << "a retained replay copies no list into a frame";

  // A change inside the boundary repaints it and leaves the pixels right, with no ancestor re-record.
  colour->setColor(Color::MakeRGBA8(200, 10, 10, 255));
  ASSERT_TRUE(window->renderFrame());

  EXPECT_EQ(window->frameStatistics().repaintedBoundaries, 1u)
    << "exactly the boundary that changed was rebuilt";
  EXPECT_GT(window->frameStatistics().frameListBytes, 0u)
    << "and the frame copied the boundary's list, which is what R3 would stop doing";
  EXPECT_TRUE(panel->hasCachedRecording());
  EXPECT_FALSE(root->hasCachedRecording());
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const uint32_t pixel = bitmap.makePixmap().getPixelARGB32(16, 16);
  EXPECT_GT((pixel >> 16) & 0xFFu, 150u) << "the boundary replayed its updated content";
  EXPECT_LT((pixel >> 0) & 0xFFu, 80u);
}

TEST(UiRetained, WithoutABoundaryTheRootStillCachesTheWholeTree) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Row);
  auto colour = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 200, 10, 255));
  colour->setBounds(Rect::MakeWH(64, 48));
  panel->addChild(colour);
  root->addChild(panel);
  window->setRoot(root);

  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->renderFrame());
  EXPECT_TRUE(root->hasCachedRecording()) << "the default path still records the whole subtree";
}

TEST(UiRetained, TableMeasuresItsCellsAtTheColumnWidth) {
  // A row height measured with no width made a wrapping cell report one line and then overflow the
  // narrow column, because layout only assigns bounds and never re-measures the cell.
  const std::string fontPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(fontPath);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  auto label = std::make_shared<TextLayer>(
    "a fairly long cell label that has to wrap inside a narrow column", font);
  Table table({ TableColumn{ .width = 80.0f } });
  table.setRows({ { label } });

  const Size measured = table.measure({ 80.0f, 0.0f });
  ASSERT_GT(measured.height, 0.0f);
  EXPECT_GE(label->layout().lines.size(), 2u)
    << "the cell was measured at the column width, so it wraps instead of overflowing";
  for (const TextLine& line: label->layout().lines) {
    EXPECT_LE(line.width, 80.5f) << "no line may be wider than the column";
  }

  table.setBounds(Rect::MakeWH(80.0f, measured.height));
  table.layout();
  EXPECT_LE(label->bounds().getWidth(), 80.5f);
}

TEST(UiRetained, ATextLeafRewrapsWhenTheParentAssignsANarrowerWidth) {
  const std::string fontPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(fontPath);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  auto label = std::make_shared<TextLayer>(
    "a label that is measured wide and then placed in a much narrower column", font);
  auto column = std::make_shared<Flex>(Axis::Column);
  column->setCrossAlign(Align::Stretch);
  column->addChild(label);

  const auto widestLine = [](const TextLayer& layer) {
    float widest = 0.0f;
    for (const TextLine& line: layer.layout().lines) widest = std::max(widest, line.width);
    return widest;
  };

  column->measure({ 400.0f, 200.0f });
  ASSERT_FALSE(label->layout().lines.empty());
  const size_t wideLines = label->layout().lines.size();
  const float wideWidth = widestLine(*label);
  ASSERT_GT(wideWidth, 60.0f) << "measured wide, so the lines are wide";

  // The parent now assigns a narrow rectangle. measure() was asked with 400, so without the bounds
  // hook the text keeps the wide line breaks and overflows the column.
  column->setBounds(Rect::MakeWH(60.0f, 200.0f));
  column->layout();
  EXPECT_GT(label->layout().lines.size(), wideLines) << "narrower bounds mean more lines";
  EXPECT_LT(widestLine(*label), wideWidth - 10.0f) << "the lines were re-broken for the new width";
  // A single word may still stick out: `overflow-wrap: normal` deliberately overflows rather than
  // breaking inside a word, so this is a sanity bound, not the column width.
  EXPECT_LE(widestLine(*label), 70.0f);
  EXPECT_LE(label->bounds().getWidth(), 60.5f);
}

TEST(UiRetained, AnUnconstrainedDockMeasuresItsCentreContent) {
  // The unconstrained size used to be two constants (240 x 180), so a dock reported the same preferred
  // size no matter what page it hosted.
  DockView dock;
  auto centre = std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 30, 40, 255));
  centre->setBounds(Rect::MakeWH(120.0f, 80.0f));
  dock.setPanel(DockView::Region::Center, centre);

  const Size measured = dock.measure({ 0.0f, 0.0f });
  EXPECT_GE(measured.width, 120.0f) << "at least the centre's natural width";
  EXPECT_LT(measured.width, 240.0f) << "and not the old hard-coded width";
  EXPECT_GE(measured.height, 80.0f);
  EXPECT_LT(measured.height, 180.0f) << "nor the old hard-coded height";

  // Sides and bottom still add their configured sizes (plus dividers) on top of the centre.
  dock.setPanel(DockView::Region::Left, std::make_shared<ColorLayer>(Color::White()), 150.0f);
  const Size withSide = dock.measure({ 0.0f, 0.0f });
  EXPECT_GT(withSide.width, measured.width) << "a side panel widens the preferred size";
  EXPECT_GE(withSide.width, 120.0f + 150.0f);
}

TEST(UiRetained, TableLaysOutAContainerCell) {
  auto cellColumn = std::make_shared<Flex>(Axis::Row);
  auto swatch = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  swatch->setBounds(Rect::MakeWH(10.0f, 10.0f));
  cellColumn->addChild(swatch);

  Table table({ TableColumn{ .width = 100.0f } });
  table.setRows({ { cellColumn } });
  const Size measured = table.measure({ 100.0f, 0.0f });
  table.setBounds(Rect::MakeWH(100.0f, measured.height));
  table.layout();

  EXPECT_GT(cellColumn->bounds().getWidth(), 0.0f);
  EXPECT_GT(swatch->bounds().getWidth(), 0.0f)
    << "the container cell arranged its child; setBounds alone does not run a layout pass";
}

TEST(UiRetained, FlexRedistributesSpaceAMaximumRefuses) {
  auto row = std::make_shared<Flex>(Axis::Row);
  auto first = std::make_shared<ColorLayer>(Color::White());
  auto second = std::make_shared<ColorLayer>(Color::White());
  LayoutParams grow;
  grow.grow = 1.0f;
  grow.basis = 0.0f;
  LayoutParams capped = grow;
  capped.maxWidth = 50.0f;
  first->setLayoutParams(capped);
  second->setLayoutParams(grow);
  row->addChild(first);
  row->addChild(second);

  row->setBounds(Rect::MakeWH(200.0f, 40.0f));
  row->measure({ 200.0f, 40.0f });
  row->layout();
  EXPECT_FLOAT_EQ(first->bounds().getWidth(), 50.0f) << "the cap holds";
  EXPECT_FLOAT_EQ(second->bounds().getWidth(), 150.0f)
    << "the space the cap refused goes to the item that can still grow";
}

TEST(UiRetained, FlexRedistributesOverflowAMinimumRefuses) {
  auto row = std::make_shared<Flex>(Axis::Row);
  auto first = std::make_shared<ColorLayer>(Color::White());
  auto second = std::make_shared<ColorLayer>(Color::White());
  LayoutParams base;
  base.basis = 100.0f;
  base.shrink = 1.0f;
  LayoutParams floored = base;
  floored.minWidth = 100.0f;
  first->setLayoutParams(base);
  second->setLayoutParams(floored);
  row->addChild(first);
  row->addChild(second);

  row->setBounds(Rect::MakeWH(120.0f, 40.0f));
  row->measure({ 120.0f, 40.0f });
  row->layout();
  EXPECT_FLOAT_EQ(second->bounds().getWidth(), 100.0f) << "the minimum holds";
  EXPECT_FLOAT_EQ(first->bounds().getWidth(), 20.0f)
    << "the overflow the floor refused is taken from the item that can still shrink";
}

TEST(UiRetained, TheStyleSheetCascadesToNestedControls) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Row);
  auto control = std::make_shared<CheckBox>("Loop", nullptr, false);
  control->setStyleName("primary");
  panel->addChild(control);
  root->addChild(panel);

  auto sheet = std::make_shared<StyleSheet>();
  Style style;
  style.base.background = Color::MakeRGBA8(30, 34, 42, 255);
  sheet->set("primary", style);

  // Installed before the tree exists: setRoot must push it down like the theme.
  window->setStyleSheet(sheet);
  window->setRoot(root);
  ASSERT_TRUE(control->resolvedStyle().has_value())
    << "a sheet installed on the window must reach a control nested two levels deep";
  EXPECT_EQ(control->resolvedStyle()->background.toR8G8B8A8()[0], 30u);

  // A control that sets its own sheet opts out of later parent sheets.
  auto own = std::make_shared<StyleSheet>();
  Style ownStyle;
  ownStyle.base.background = Color::MakeRGBA8(200, 10, 10, 255);
  own->set("primary", ownStyle);
  control->setStyleSheet(own);
  EXPECT_EQ(control->resolvedStyle()->background.toR8G8B8A8()[0], 200u);

  auto replacement = std::make_shared<StyleSheet>();
  Style replacementStyle;
  replacementStyle.base.background = Color::MakeRGBA8(1, 2, 3, 255);
  replacement->set("primary", replacementStyle);
  window->setStyleSheet(replacement);
  EXPECT_EQ(control->resolvedStyle()->background.toR8G8B8A8()[0], 200u)
    << "an explicit sheet wins over the inherited one";

  // A child added after the sheet was installed inherits it too.
  auto late = std::make_shared<CheckBox>("Late", nullptr, false);
  late->setStyleName("primary");
  panel->addChild(late);
  ASSERT_TRUE(late->resolvedStyle().has_value());
  EXPECT_EQ(late->resolvedStyle()->background.toR8G8B8A8()[0], 1u);
}

TEST(UiRetained, ASheetRuleCanAddressAControlByItsKind) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto sheet = std::make_shared<StyleSheet>();
  Style buttonStyle;
  buttonStyle.base.background = Color::MakeRGBA8(11, 22, 33, 255);
  sheet->set("button", buttonStyle);
  Style panelStyle;
  panelStyle.base.background = Color::MakeRGBA8(44, 55, 66, 255);
  sheet->set("primary", panelStyle);
  window->setStyleSheet(sheet);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto plain = std::make_shared<Button>("Plain", nullptr);
  root->addChild(plain);
  window->setRoot(root);

  ASSERT_TRUE(plain->resolvedStyle().has_value())
    << "a sheet rule named after the control kind must reach controls the page never named";
  EXPECT_EQ(plain->resolvedStyle()->background.toR8G8B8A8()[0], 11u);

  // An explicit name still wins over the kind default.
  plain->setStyleName("primary");
  EXPECT_EQ(plain->resolvedStyle()->background.toR8G8B8A8()[0], 44u);
}

TEST(UiRetained, ElementPropsCanNameAStyle) {
  Reconciler reconciler;
  ElementProps props;
  props.styleName = std::string("primary");
  auto root = reconciler.mount(components::WithProps(
    std::make_shared<ColorElement>(Color::White(), "dot"), props, "wrapper"));
  ASSERT_NE(root, nullptr);

  auto sheet = std::make_shared<StyleSheet>();
  Style style;
  style.base.background = Color::MakeRGBA8(7, 8, 9, 255);
  sheet->set("primary", style);
  root->setStyleSheet(sheet);

  ASSERT_TRUE(root->resolvedStyle().has_value()) << "the props wrapper named the layer's style";
  EXPECT_EQ(root->resolvedStyle()->background.toR8G8B8A8()[0], 7u);

  // Changing only the name is a props change, so the wrapper must be re-applied.
  props.styleName = std::string("secondary");
  reconciler.update(components::WithProps(
    std::make_shared<ColorElement>(Color::White(), "dot"), props, "wrapper"));
  EXPECT_EQ(reconciler.lastAppliedNodeCount(), 1u);
  EXPECT_EQ(root->styleName(), "secondary");
}

TEST(UiRetained, ADisabledControlIsInertAndReportsTheDisabledState) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto box = std::make_shared<CheckBox>("Loop", nullptr, false);
  root->addChild(box);
  window->setRoot(root);
  box->setBounds(Rect::MakeWH(40.0f, 20.0f));

  auto sheet = std::make_shared<StyleSheet>();
  Style style;
  style.base.background = Color::MakeRGBA8(10, 20, 30, 255);
  style.variant(stateFlag(ControlState::Disabled), StylePatch{ .background = Color::MakeRGBA8(90, 90, 90, 255) });
  sheet->set("checkbox", style);
  window->setStyleSheet(sheet);
  ASSERT_TRUE(box->resolvedStyle().has_value());
  EXPECT_EQ(box->resolvedStyle()->background.toR8G8B8A8()[0], 10u);

  box->setEnabled(false);
  EXPECT_FALSE(box->enabled());
  EXPECT_NE(box->styleStates() & stateFlag(ControlState::Disabled), 0u)
    << "a disabled control reports the state without implementing anything";
  EXPECT_EQ(box->resolvedStyle()->background.toR8G8B8A8()[0], 90u)
    << "the disabled variant of the kind's rule applies";

  // Input is ignored: a click on a disabled checkbox does not toggle it, and it cannot take focus.
  PointerEvent click;
  click.position = { 5.0f, 5.0f };
  click.button = PointerButton::Primary;
  window->focusLayer(box.get());
  EXPECT_EQ(window->focusedLayer(), nullptr) << "a disabled control must not take focus";
  root->onPointerDown(click);
  root->onPointerUp(click);
  EXPECT_FALSE(box->checked());

  // The accessibility snapshot reports the state too, and marks it unfocusable.
  const AccessibilityNode tree = window->accessibilityTree();
  const AccessibilityNode* node = &tree;
  std::vector<const AccessibilityNode*> pending{ &tree };
  bool found = false;
  while (!pending.empty()) {
    node = pending.back();
    pending.pop_back();
    if (node->name == "Loop") { found = true; break; }
    for (const AccessibilityNode& child: node->children) pending.push_back(&child);
  }
  ASSERT_TRUE(found) << "the checkbox is in the accessibility tree";
  EXPECT_FALSE(node->enabled) << "a disabled control must be announced as disabled";
  EXPECT_FALSE(node->focusable);

  // Re-enabling restores both.
  box->setEnabled(true);
  EXPECT_EQ(box->styleStates() & stateFlag(ControlState::Disabled), 0u);
  window->focusLayer(box.get());
  EXPECT_EQ(window->focusedLayer(), box.get());
}

TEST(UiRetained, BackgroundColourTransitionsBetweenStates) {
  Layer layer;
  layer.setBounds(Rect::MakeWH(20.0f, 10.0f));
  const Color from = Color::MakeRGBA8(10, 20, 30, 255);
  const Color to = Color::MakeRGBA8(200, 100, 50, 255);
  layer.setBackgroundColor(from);

  layer.setTransition(std::chrono::milliseconds(100), Easing::Linear);
  layer.setBackgroundColor(to);
  EXPECT_TRUE(layer.hasBackgroundAnimation()) << "an implicit transition starts a colour animation";
  EXPECT_TRUE(layer.hasRunningAnimation()) << "the frame loop has to keep running";
  EXPECT_FALSE(layer.backgroundColor() == to) << "the colour does not snap";

  const auto start = std::chrono::steady_clock::now();
  layer.onFrame(start + std::chrono::milliseconds(50));
  const Color middle = layer.backgroundColor();
  EXPECT_GT(middle.red, from.red);
  EXPECT_LT(middle.red, to.red);

  layer.onFrame(start + std::chrono::milliseconds(120));
  EXPECT_FALSE(layer.hasBackgroundAnimation()) << "the animation ends when its duration elapsed";
  EXPECT_TRUE(layer.backgroundColor() == to) << "and lands exactly on the target";
  EXPECT_FALSE(layer.hasRunningAnimation());
}

TEST(UiRetained, AZeroDurationColourTransitionSnaps) {
  Layer layer;
  layer.setBounds(Rect::MakeWH(20.0f, 10.0f));
  layer.setBackgroundColor(Color::MakeRGBA8(10, 20, 30, 255));
  layer.setTransition(std::chrono::milliseconds(0));
  const Color to = Color::MakeRGBA8(200, 100, 50, 255);
  layer.setBackgroundColor(to);
  EXPECT_FALSE(layer.hasBackgroundAnimation());
  EXPECT_TRUE(layer.backgroundColor() == to);
}

TEST(UiRetained, FrameworkStringsComeFromTheTranslator) {
  auto translator = std::make_shared<Translator>(Locale::parse("de"));
  translator->addCatalog("ui.table.cell=Zeile {row}, Spalte {column}\n"
    "ui.textinput.cut=Ausschneiden\n"
    "ui.textinput.copy=Kopieren\n");

  Theme theme = Theme::dark();
  theme.translator = translator;

  Window::Options options;
  options.headless = true;
  options.width = 96;
  options.height = 64;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  window->setTheme(theme);

  auto root = std::make_shared<Flex>(Axis::Column);
  // An unnamed cell: a cell whose own text already names it keeps that name (an explicit name wins),
  // so the positional label has to be checked on one that has none.
  auto cell = std::make_shared<ColorLayer>(Color::White());
  auto table = std::make_shared<Table>(std::vector<TableColumn>{ TableColumn{ .width = 80.0f } });
  table->setRows({ { cell } });
  root->addChild(table);
  window->setRoot(root);

  // The table names its cells through the theme's translator with the row/column placeholders filled.
  table->measure({ 80.0f, 0.0f });
  table->setBounds(Rect::MakeWH(80.0f, table->measuredSize().height));
  table->layout();
  const AccessibilityNode tree = window->accessibilityTree();
  std::vector<const AccessibilityNode*> pending{ &tree };
  std::string cellName;
  while (!pending.empty()) {
    const AccessibilityNode* node = pending.back();
    pending.pop_back();
    if (node->name.rfind("Zeile", 0) == 0) cellName = node->name;
    for (const AccessibilityNode& child: node->children) pending.push_back(&child);
  }

  EXPECT_EQ(cellName, "Zeile 1, Spalte 1") << "the cell label is translated and interpolated";

  // A text field builds its context menu on first use, so the labels can come from the translator.
  const std::string fontPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(fontPath);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  auto field = std::make_shared<TextInput>("abc", font);
  root->addChild(field);
  field->setBounds(Rect::MakeWH(60.0f, 20.0f));
  PointerEvent right;
  right.position = { 5.0f, 5.0f };
  right.button = PointerButton::Secondary;
  field->onPointerDown(right);

  Menu* menu = nullptr;
  for (const std::shared_ptr<Layer>& child: field->children()) {
    if (child->isPopup()) menu = static_cast<Menu*>(child.get());
  }
  ASSERT_NE(menu, nullptr) << "the context menu exists after a secondary click";
  EXPECT_EQ(menu->itemLabel(0), "Ausschneiden");
  EXPECT_EQ(menu->itemLabel(1), "Kopieren");
  EXPECT_EQ(menu->itemLabel(2), "Paste") << "a missing key keeps the English fallback";
}

TEST(UiRetained, ADirtyBoundaryRepaintsItsWholeRectangle) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  const auto ratioAfterTouching = [](const bool boundary) {
    Window::Options options;
    options.headless = true;
    options.width = 100;
    options.height = 100;
    auto window = Window::create(options);
    auto root = std::make_shared<Flex>(Axis::Column);
    auto panel = std::make_shared<Flex>(Axis::Row);
    panel->setRepaintBoundary(boundary);
    // A floor in the parent's layout, because the parent lays the panel out and would otherwise shrink
    // it to its content (the 10x10 dot), which would make boundary and leaf damage identical.
    LayoutParams fixed;
    fixed.minWidth = 80.0f;
    fixed.minHeight = 80.0f;
    panel->setLayoutParams(fixed);
    auto dot = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
    dot->setBounds(Rect::MakeWH(10.0f, 10.0f));
    panel->addChild(dot);
    root->addChild(panel);
    window->setRoot(root);
    window->renderFrame();
    window->renderFrame();
    dot->setColor(Color::MakeRGBA8(9, 9, 9, 255));

    window->renderFrame();
    return window->frameStatistics().damagedAreaRatio;
  };

  // A boundary is the repaint unit: the damage it reports is its own rectangle, not the small leaf
  // that changed, so the region stays coherent (and a compositor could replay just that rectangle).
  EXPECT_GT(ratioAfterTouching(true), 0.5) << "the boundary's whole rectangle is damaged";
  EXPECT_LT(ratioAfterTouching(false), 0.1) << "without a boundary only the leaf is";
}

namespace {

/** Depth-first search by accessible name; returns nullptr when the tree has no such node. */
const AccessibilityNode* findNode(const AccessibilityNode& node, const std::string& name) {
  if (node.name == name) return &node;
  for (const AccessibilityNode& child: node.children) {
    if (const AccessibilityNode* found = findNode(child, name)) return found;
  }
  return nullptr;
}

} // namespace

TEST(UiRetained, ASliderExposesItsRangeNotAValue) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 80;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto slider = std::make_shared<Slider>("Volume", nullptr, 0.0, 100.0, 25.0);
  slider->setAccessibleName("Volume slider");
  root->addChild(slider);
  window->setRoot(root);

  const AccessibilityNode tree = window->accessibilityTree();
  const AccessibilityNode* node = findNode(tree, "Volume slider");
  ASSERT_NE(node, nullptr) << "the slider is in the accessibility tree";
  ASSERT_TRUE(node->range.has_value()) << "a slider reports its numeric range";
  EXPECT_DOUBLE_EQ(node->range->value, 25.0);
  EXPECT_DOUBLE_EQ(node->range->minimum, 0.0);
  EXPECT_DOUBLE_EQ(node->range->maximum, 100.0);

  const AccessibilityPattern patterns = accessibilityPatterns(*node);
  EXPECT_TRUE(hasAccessibilityPattern(patterns, AccessibilityPattern::RangeValue));
  EXPECT_FALSE(hasAccessibilityPattern(patterns, AccessibilityPattern::Value))
    << "a range control must not also look like an editable text value";

  // The UIA range provider hands the control a decimal string; the control parses its own units.
  ASSERT_TRUE(static_cast<Layer&>(*slider).performAccessibilityAction(
    AccessibilityAction::SetValue, "75"));
  EXPECT_DOUBLE_EQ(slider->value(), 75.0);
  EXPECT_FALSE(static_cast<Layer&>(*slider).performAccessibilityAction(
    AccessibilityAction::SetValue, "not-a-number"));
}

TEST(UiRetained, AScrollViewExposesAndObeysItsScrollPattern) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 120;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto scroll = std::make_shared<ScrollView>();
  auto content = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 200, 0, 255));
  content->setBounds(Rect::MakeWH(64.0f, 400.0f));
  scroll->setContent(content);
  scroll->setAccessibleName("Page");
  root->addChild(scroll);
  window->setRoot(root);
  scroll->setBounds(Rect::MakeWH(64.0f, 64.0f));
  scroll->measure({ 64.0f, 64.0f });
  scroll->layout();

  const AccessibilityNode tree = window->accessibilityTree();
  const AccessibilityNode* node = findNode(tree, "Page");
  ASSERT_NE(node, nullptr) << "the viewport is named and therefore in the tree";
  ASSERT_EQ(node->role, AccessibilityRole::ScrollView);
  ASSERT_TRUE(node->scroll.has_value()) << "a viewport reports its scroll state";
  EXPECT_TRUE(node->scroll->canScrollVertically);
  EXPECT_DOUBLE_EQ(node->scroll->verticalPercent, 0.0);
  EXPECT_LT(node->scroll->verticalViewSize, 1.0);
  EXPECT_TRUE(hasAccessibilityPattern(accessibilityPatterns(*node), AccessibilityPattern::Scroll));

  // The platform bridge sends a command string; the control is what knows its units.
  ASSERT_TRUE(scroll->performAccessibilityAction(AccessibilityAction::Scroll, "pageDown"));
  EXPECT_GT(scroll->scrollY(), 0.0f);
  ASSERT_TRUE(scroll->performAccessibilityAction(AccessibilityAction::Scroll, "set:0,1"));
  EXPECT_TRUE(scroll->accessibleScroll()->verticalPercent > 0.9);
  EXPECT_FALSE(scroll->performAccessibilityAction(AccessibilityAction::Scroll, "nonsense"));
}

TEST(UiRetained, ATableExposesAGridAndItsCellsReportPositions) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto cell = [](const char* name) {
    auto layer = std::make_shared<ColorLayer>(Color::White());
    layer->setAccessibleName(name);
    return layer;
  };
  auto table = std::make_shared<Table>(
    std::vector<TableColumn>{ TableColumn{ .width = 60.0f }, TableColumn{ .width = 60.0f } });
  table->setRows({ { cell("a1"), cell("a2") }, { cell("b1"), cell("b2") } });
  table->setAccessibleName("Scores");
  root->addChild(table);
  window->setRoot(root);

  const AccessibilityNode tree = window->accessibilityTree();
  const AccessibilityNode* node = findNode(tree, "Scores");
  ASSERT_NE(node, nullptr);
  ASSERT_TRUE(node->grid.has_value());
  EXPECT_EQ(node->grid->rows, 2);
  EXPECT_EQ(node->grid->columns, 2);
  EXPECT_TRUE(hasAccessibilityPattern(accessibilityPatterns(*node), AccessibilityPattern::Grid));

  const AccessibilityNode* a2 = findNode(tree, "a2");
  ASSERT_NE(a2, nullptr);
  ASSERT_TRUE(a2->grid.has_value()) << "a cell knows which grid position it occupies";
  EXPECT_EQ(a2->grid->row, 0);
  EXPECT_EQ(a2->grid->column, 1);
  EXPECT_TRUE(hasAccessibilityPattern(accessibilityPatterns(*a2), AccessibilityPattern::GridItem));
}

TEST(UiRetained, AccessibilityAnnouncesStructureAndEnabledChanges) {
  Window::Options options;
  options.headless = true;
  options.width = 120;
  options.height = 80;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  window->setRoot(root);

  // Replace the window's notifier so the test observes the same events the platform bridge would.
  std::vector<AccessibilityEvent> events;
  setAccessibilityNotifier(window.get(),
    [&events](Layer&, const AccessibilityEvent event) { events.push_back(event); });

  auto child = std::make_shared<ColorLayer>(Color::White());
  root->addChild(child);
  ASSERT_FALSE(events.empty());
  EXPECT_EQ(events.back(), AccessibilityEvent::ChildrenAdded)
    << "adding a child tells assistive technology the tree changed";

  child->setEnabled(false);
  ASSERT_FALSE(events.empty());
  EXPECT_EQ(events.back(), AccessibilityEvent::EnabledChanged);

  root->removeChild(child);
  ASSERT_FALSE(events.empty());
  EXPECT_EQ(events.back(), AccessibilityEvent::ChildrenRemoved);

  setAccessibilityNotifier(window.get(), {});
}

TEST(UiRetained, ATextFieldExposesItsSelectionInUtf16Units) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 60;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  // "ab😀cd": the emoji is four UTF-8 bytes but two UTF-16 code units, which is exactly the case a
  // byte-offset bridge gets wrong.
  // Written as raw UTF-8 bytes so the literal does not depend on the source encoding: U+1F600 is four
  // bytes and two UTF-16 code units.
  const std::string sample = std::string("ab") + "\xF0\x9F\x98\x80" + "cd";
  auto field = std::make_shared<TextInput>(sample, nullptr);
  root->addChild(field);
  window->setRoot(root);
  field->selectAll();

  const AccessibilityNode tree = window->accessibilityTree();
  const AccessibilityNode* text = nullptr;
  std::function<void(const AccessibilityNode&)> walk = [&](const AccessibilityNode& current) {
    if (current.role == AccessibilityRole::TextField) text = &current;
    for (const AccessibilityNode& child: current.children) walk(child);
  };
  walk(tree);
  ASSERT_NE(text, nullptr) << "the edit is in the accessibility tree";
  ASSERT_TRUE(text->textSelection.has_value()) << "an editable field reports its selection";
  EXPECT_EQ(text->textSelection->start, 0);
  EXPECT_EQ(text->textSelection->end, 6) << "two ASCII letters, one astral pair, two more letters";

  const AccessibilityPattern patterns = accessibilityPatterns(*text);
  EXPECT_TRUE(hasAccessibilityPattern(patterns, AccessibilityPattern::Text));
  EXPECT_TRUE(hasAccessibilityPattern(patterns, AccessibilityPattern::Value));

  // A bridge selects through the same action; the offsets are UTF-16 units, the control converts back.
  ASSERT_TRUE(static_cast<Layer&>(*field).performAccessibilityAction(AccessibilityAction::Select, "2,4"));
  EXPECT_EQ(field->selectionStart(), 2u) << "the astral character starts after two bytes";
  EXPECT_EQ(field->selectionEnd(), 6u) << "and occupies four";
  EXPECT_FALSE(static_cast<Layer&>(*field).performAccessibilityAction(AccessibilityAction::Select, "not-a-range"));
}

namespace {

/** Counts measure calls so the cache behaviour is observable. */
class CountingMeasureLayer final : public Layer {
 public:
  Size measure(const Size& available) override {
    ++calls;
    return Size{ 30.0f, 20.0f };
  }
  int calls = 0;
};

} // namespace

TEST(UiRetained, ConstraintsSpellUnboundedExplicitly) {
  const Constraints unbounded = Constraints::unbounded();
  EXPECT_FALSE(unbounded.hasBoundedWidth());
  EXPECT_FALSE(unbounded.hasBoundedHeight());
  EXPECT_EQ(unbounded.availableSize().width, 0.0f) << "the legacy spelling of unbounded";
  EXPECT_EQ(unbounded.availableSize().height, 0.0f);

  const Constraints tight = Constraints::tight(Size{ 120.0f, 40.0f });
  EXPECT_TRUE(tight.hasBoundedWidth());
  EXPECT_EQ(tight.maxWidth, 120.0f);
  EXPECT_EQ(tight.maxHeight, 40.0f);
  EXPECT_EQ(tight.minWidth, 120.0f) << "a tight box is also a minimum";
}

TEST(UiRetained, ConstraintsClampTheMinimumAndCachePerBox) {
  CountingMeasureLayer layer;
  Constraints box;
  box.minWidth = 50.0f;
  box.minHeight = 60.0f;
  const Size measured = layer.measureWithConstraints(box);
  EXPECT_FLOAT_EQ(measured.width, 50.0f) << "the natural width is below the minimum";
  EXPECT_FLOAT_EQ(measured.height, 60.0f);
  EXPECT_EQ(layer.calls, 1) << "the direct call is not cached";

  // The same box hits the cache; a different one does not, and the two cache kinds do not collide.
  EXPECT_FLOAT_EQ(layer.measureCachedWithConstraints(box).width, 50.0f);
  EXPECT_EQ(layer.calls, 2) << "the first cached request still measures";
  EXPECT_FLOAT_EQ(layer.measureCachedWithConstraints(box).width, 50.0f);
  EXPECT_EQ(layer.calls, 2) << "the second identical request is served from the cache";

  Constraints wider;
  wider.minWidth = 80.0f;
  EXPECT_FLOAT_EQ(layer.measureCachedWithConstraints(wider).width, 80.0f);
  EXPECT_EQ(layer.calls, 3);

  EXPECT_FLOAT_EQ(layer.measureCached(Size{ 10.0f, 10.0f }).width, 30.0f)
    << "the legacy size cache is a separate entry";
}

TEST(UiRetained, AScrollViewAbsorbsLayoutChangesBelowItself) {
  const std::string fontPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(fontPath);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);

  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 200;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto page = std::make_shared<Flex>(Axis::Column);
  auto scroll = std::make_shared<ScrollView>();
  // A viewport is a boundary only when the parent pinned its size; percentages do that.
  LayoutParams fixed;
  fixed.widthPercent = 1.0f;
  fixed.heightPercent = 1.0f;
  scroll->setLayoutParams(fixed);
  auto content = std::make_shared<Flex>(Axis::Column);
  auto label = std::make_shared<TextLayer>("short", font);
  content->addChild(label);
  scroll->setContent(content);
  page->addChild(scroll);
  root->addChild(page);
  window->setRoot(root);
  window->renderFrame();
  window->renderFrame();

  // A text change inside the viewport cannot change the viewport's size, so the layout pass must not
  // treat the page above it as stale.
  label->setText("a much longer label that needs more room than the short one did");
  EXPECT_FALSE(root->needsLayout()) << "the boundary absorbed the change";
  EXPECT_TRUE(scroll->subtreeNeedsLayout()) << "but the pass still has to reach the boundary";

  ASSERT_TRUE(window->renderFrame());
  const FrameStatistics stats = window->frameStatistics();
  EXPECT_LE(stats.layoutDirtyNodes, 4u)
    << "only the label, its content box and the viewport are laid out";
  // Flags rather than timings: a three-node layout can finish inside the clock's resolution.
  EXPECT_FALSE(root->needsLayout());
  EXPECT_FALSE(scroll->needsLayout()) << "the pass reached the boundary";
  // Growing the content changes its bounds, and a size change legitimately asks for another pass; it
  // stays inside the boundary, which is the property under test.
  EXPECT_FALSE(root->needsLayout()) << "the second pass is still absorbed";
  ASSERT_TRUE(window->renderFrame());
  EXPECT_FALSE(label->needsLayout()) << "and settles after the size change was paid off";
  EXPECT_FALSE(root->subtreeNeedsLayout());
}

TEST(UiRetained, APlainFlexStillPropagatesLayoutToTheRoot) {
  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Column);
  auto label = std::make_shared<ColorLayer>(Color::White());
  panel->addChild(label);
  root->addChild(panel);

  root->layout();
  label->setNeedsLayout();
  EXPECT_TRUE(panel->needsLayout())
    << "a container whose arrangement depends on its children must be re-arranged";
  EXPECT_TRUE(root->needsLayout());
}

TEST(UiRetained, AnUnfixedScrollViewStillReportsItsMeasurement) {
  // The viewport's preferred height comes from its content, so an unfixed viewport must keep
  // propagating: the page above may need a new measurement when the content grows.
  const std::string fontPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(fontPath);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto scroll = std::make_shared<ScrollView>();
  auto content = std::make_shared<Flex>(Axis::Column);
  auto label = std::make_shared<TextLayer>("short", font);
  content->addChild(label);
  scroll->setContent(content);
  root->addChild(scroll);

  root->layout();
  EXPECT_FALSE(scroll->isLayoutBoundary())
    << "without a fixed size the viewport's measurement depends on its content";
  label->setNeedsLayout();
  EXPECT_TRUE(root->needsLayout()) << "so the change has to reach the root";
}

TEST(UiRetained, AFlexWithAFixedParentSizeAbsorbsItsChildren) {
  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Column);
  LayoutParams fixed;
  fixed.widthPercent = 1.0f;
  fixed.heightPercent = 1.0f;
  panel->setLayoutParams(fixed);
  auto label = std::make_shared<ColorLayer>(Color::White());
  panel->addChild(label);
  root->addChild(panel);

  root->layout();
  ASSERT_TRUE(panel->isLayoutBoundary());
  root->layout();
  label->setNeedsLayout();
  EXPECT_TRUE(panel->needsLayout());
  EXPECT_FALSE(root->needsLayout()) << "the panel's fixed size stops the change";
  EXPECT_TRUE(root->subtreeNeedsLayout()) << "but the pass still has to visit it";
}

TEST(UiRetained, AFixedTableAbsorbsChangesInItsCells) {
  auto root = std::make_shared<Flex>(Axis::Column);
  auto table = std::make_shared<Table>(
    std::vector<TableColumn>{ TableColumn{ .width = 60.0f } });
  auto cell = std::make_shared<Flex>(Axis::Column);
  auto label = std::make_shared<ColorLayer>(Color::White());
  cell->addChild(label);
  table->setRows({ { cell } });
  LayoutParams fixed;
  fixed.widthPercent = 1.0f;
  fixed.heightPercent = 1.0f;
  table->setLayoutParams(fixed);
  root->addChild(table);
  root->layout();

  ASSERT_TRUE(table->isLayoutBoundary());
  label->setNeedsLayout();
  EXPECT_TRUE(table->needsLayout());
  EXPECT_TRUE(cell->needsLayout()) << "everything at or below the boundary re-arranges";
  EXPECT_FALSE(root->needsLayout()) << "the page above the table is untouched";
  EXPECT_TRUE(root->subtreeNeedsLayout()) << "but it still runs a pass to reach the table";
}

TEST(UiRetained, APlainGridAndDockOnlyBoundWhenFixed) {
  auto grid = std::make_shared<Grid>();
  EXPECT_FALSE(grid->isLayoutBoundary()) << "an unpinned grid sizes itself from its cells";
  LayoutParams fixed;
  fixed.widthPercent = 1.0f;
  fixed.heightPercent = 1.0f;
  grid->setLayoutParams(fixed);
  EXPECT_TRUE(grid->isLayoutBoundary());

  auto dock = std::make_shared<DockView>();
  EXPECT_FALSE(dock->isLayoutBoundary());
  dock->setLayoutParams(fixed);
  EXPECT_TRUE(dock->isLayoutBoundary());
}

TEST(UiRetained, AtsPiInterfacesFollowTheSamePatternSetAsWindows) {
  // The Linux mapping layer is the bridge's table of contents; it has to agree with the Windows
  // provider about what a control offers, which is why both read `accessibilityPatterns`.
  auto has = [](const std::vector<std::string>& interfaces, const char* name) {
    return std::find(interfaces.begin(), interfaces.end(), name) != interfaces.end();
  };

  AccessibilityNode slider;
  slider.role = AccessibilityRole::Slider;
  slider.actions = AccessibilityAction::SetValue;
  slider.range = AccessibleRange{ 0.5, 0.0, 1.0, 0.05 };
  const auto sliderInterfaces = atspiInterfaces(slider);
  EXPECT_TRUE(has(sliderInterfaces, "org.a11y.atspi.Value")) << "a range control is a Value";
  EXPECT_FALSE(has(sliderInterfaces, "org.a11y.atspi.Text"));

  AccessibilityNode field;
  field.role = AccessibilityRole::TextField;
  field.actions = AccessibilityAction::SetValue;
  field.editable = true;
  field.textSelection = AccessibleTextSelection{ 0, 3 };
  const auto fieldInterfaces = atspiInterfaces(field);
  EXPECT_TRUE(has(fieldInterfaces, "org.a11y.atspi.Text"));
  EXPECT_TRUE(has(fieldInterfaces, "org.a11y.atspi.EditableText"));
  EXPECT_TRUE(has(fieldInterfaces, "org.a11y.atspi.Value"));

  AccessibilityNode table;
  table.role = AccessibilityRole::Table;
  table.grid = AccessibleGrid{ 2, 3 };
  const auto tableInterfaces = atspiInterfaces(table);
  EXPECT_TRUE(has(tableInterfaces, "org.a11y.atspi.Table"));
  EXPECT_TRUE(has(tableInterfaces, "org.a11y.atspi.Selection")) << "a table owns a selection";

  AccessibilityNode cell;
  cell.role = AccessibilityRole::ListItem;
  cell.grid = AccessibleGrid{ 2, 3, 0, 1 };
  const auto cellInterfaces = atspiInterfaces(cell);
  EXPECT_FALSE(has(cellInterfaces, "org.a11y.atspi.Table"))
    << "a cell is an Accessible child, not a second table";
}

TEST(UiRetained, ScrollViewMeasuresItsContentOncePerLayoutPass) {
  auto view = std::make_shared<ScrollView>();
  auto content = std::make_shared<CountingMeasureLayer>();
  view->setContent(content);
  view->setBounds(Rect::MakeWH(100.0f, 80.0f));

  // measure and layout ask for the same width; the content's own measure cache should serve the second
  // question instead of walking its children again.
  view->measure({ 100.0f, 0.0f });
  const int afterMeasure = content->calls;
  view->layout();
  EXPECT_EQ(content->calls, afterMeasure)
    << "the layout pass reused the measurement the measure pass just took";
  EXPECT_EQ(afterMeasure, 1) << "and only asked the content once to begin with";
}

TEST(UiRetained, ABoundaryLendsItsRecordedLists) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Row);
  panel->setRepaintBoundary(true);
  auto colour = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 200, 10, 255));
  colour->setBounds(Rect::MakeWH(64, 48));
  panel->addChild(colour);
  root->addChild(panel);
  window->setRoot(root);

  EXPECT_TRUE(panel->borrowRecording().empty()) << "nothing recorded before the first frame";
  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->renderFrame());

  const auto lent = panel->borrowRecording();
  ASSERT_FALSE(lent.empty()) << "a boundary hands out the lists it recorded";
  EXPECT_EQ(lent.size(), 2u) << "its own paint plus one child segment";
  std::size_t nonEmpty = 0;
  for (const auto& list: lent) {
    ASSERT_NE(list, nullptr);
    if (!list->commands.empty()) ++nonEmpty;
  }
  EXPECT_GE(nonEmpty, 1u)
    << "at least the child segment carries the panel's content; the own-paint segment may be empty";

  // Immutability is the contract: a change below the boundary replaces the child segment rather than
  // mutating a list an already-assembled frame could still hold.
  const auto before = lent.back();
  colour->setColor(Color::MakeRGBA8(200, 10, 10, 255));
  ASSERT_TRUE(window->renderFrame());
  const auto after = panel->borrowRecording();
  ASSERT_EQ(after.size(), 2u);
  EXPECT_NE(after.back().get(), before.get()) << "the patched segment is a new list";
  EXPECT_GT(before->commands.size(), 0u) << "and the old list is still intact for whoever holds it";

  // A layer with no recording lends nothing rather than a stale vector.
  auto plain = std::make_shared<ColorLayer>(Color::White());
  EXPECT_TRUE(plain->borrowRecording().empty());
}

TEST(UiRetained, AFramePlanListsTheBoundariesInPaintOrder) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 96;
  options.height = 64;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto first = std::make_shared<Flex>(Axis::Row);
  first->setRepaintBoundary(true);
  auto firstFill = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  firstFill->setBounds(Rect::MakeWH(96.0f, 24.0f));
  first->addChild(firstFill);
  auto middle = std::make_shared<Flex>(Axis::Row);
  auto nested = std::make_shared<Flex>(Axis::Row);
  nested->setRepaintBoundary(true);
  auto nestedFill = std::make_shared<ColorLayer>(Color::MakeRGBA8(4, 5, 6, 255));
  nestedFill->setBounds(Rect::MakeWH(96.0f, 16.0f));
  nested->addChild(nestedFill);
  middle->addChild(nested);
  // A zero z-index keeps insertion order; the second boundary is nested inside a plain container, so a
  // plan that only looked at direct children would miss it.
  root->addChild(first);
  root->addChild(middle);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->renderFrame());

  const FramePlan plan = collectFramePlan(*root);
  ASSERT_EQ(plan.boundaries.size(), 2u);
  EXPECT_EQ(plan.boundaries[0].boundary, first.get());
  EXPECT_EQ(plan.boundaries[1].boundary, nested.get())
    << "a boundary nested under a plain container is still part of the frame";
  EXPECT_EQ(plan.order().size(), 2u);
  EXPECT_EQ(plan.order()[0], first.get());

  // The plan borrows: its lists are the same objects `borrowRecording` hands out.
  const auto lent = first->borrowRecording();
  ASSERT_FALSE(lent.empty());
  ASSERT_FALSE(plan.boundaries[0].lists.empty());
  EXPECT_EQ(plan.boundaries[0].lists.front().get(), lent.front().get());
  EXPECT_EQ(plan.borrowedLists, 4u) << "two boundaries, two segments each";
  EXPECT_GT(plan.borrowedBytes, 0u);

  // A tree with no boundary produces an empty plan, not a plan of one implicit root item.
  auto plainRoot = std::make_shared<Flex>(Axis::Column);
  plainRoot->addChild(std::make_shared<ColorLayer>(Color::White()));
  EXPECT_TRUE(collectFramePlan(*plainRoot).empty());
}

TEST(UiRetained, APlanModeFrameSkipsBoundariesAndCollectsThem) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 96;
  options.height = 64;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Row);
  panel->setRepaintBoundary(true);
  auto fill = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 200, 10, 255));
  fill->setBounds(Rect::MakeWH(96.0f, 24.0f));
  panel->addChild(fill);
  auto outside = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 0, 200, 255));
  outside->setBounds(Rect::MakeWH(96.0f, 12.0f));
  root->addChild(panel);
  root->addChild(outside);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->renderFrame());
  ASSERT_FALSE(panel->borrowRecording().empty());

  // Recording the frame in plan mode: the boundary is collected, not recorded. The same tree recorded
  // normally puts both the boundary's content and the outside content in the list.
  const auto record = [&](std::vector<RenderContext::PlannedItem>* plan) {
    auto list = std::make_shared<graphics::gpu::DrawList>();
    std::unique_ptr<graphics::Canvas> canvas = graphics::Canvas::createRecordingBackend(
      96, 64, nullptr, list);
    if (canvas == nullptr) {
      return std::make_pair(std::size_t{ 0 }, std::make_shared<graphics::gpu::DrawList>());
    }
    Canvas uiCanvas = detail::makeCanvas(*canvas);
    RenderContext context(uiCanvas, 1.0f, nullptr, Size{ 96.0f, 64.0f }, nullptr);
    context.setPlanSink(plan);
    root->draw(context);
    return std::make_pair(list->commands.size(), list);
  };

  std::vector<RenderContext::PlannedItem> collected;
  const auto planned = record(&collected);
  ASSERT_EQ(collected.size(), 1u) << "the boundary was collected instead of drawn";
  EXPECT_EQ(collected.front().layer, panel.get());
  const auto full = record(nullptr);
  EXPECT_LT(planned.first, full.first)
    << "the plan-mode list leaves the boundary's commands out";
  EXPECT_GT(planned.first, 0u) << "but the content outside the boundary is still recorded";
}

TEST(UiRetained, TheFramePlanPathsPaintsTheSamePixels) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 96;
  options.height = 64;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Row);
  panel->setRepaintBoundary(true);
  auto fill = std::make_shared<ColorLayer>(Color::MakeRGBA8(200, 20, 20, 255));
  fill->setBounds(Rect::MakeWH(96.0f, 24.0f));
  panel->addChild(fill);
  auto outside = std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 20, 200, 255));
  outside->setBounds(Rect::MakeWH(96.0f, 12.0f));
  root->addChild(panel);
  root->addChild(outside);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->renderFrame());


  // A full repaint through the plan, then the same repaint with the plan disabled: the two frames have
  // to be the same picture, because the plan only changes *where* the boundary's commands came from.
  // Let the colour change settle first: a dirty boundary is drawn (that is what patches its list), so
  // only a clean one is lent. Then a full repaint with nothing dirty is the plan's frame.
  fill->setColor(Color::MakeRGBA8(30, 220, 30, 255));
  root->setNeedsPaint();
  ASSERT_TRUE(window->renderFrame());

  graphics::Bitmap planned;
  {
    const Window::FramePlanScope planScope(true);
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    EXPECT_GT(window->frameStatistics().framePlanItems, 0u)
      << "the frame was assembled from boundary lists";
    ASSERT_TRUE(window->snapshot(&planned));
  }

  graphics::Bitmap plain;
  {
    const Window::FramePlanScope planScope(false);
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    EXPECT_EQ(window->frameStatistics().framePlanItems, 0u);
    ASSERT_TRUE(window->snapshot(&plain));
  }

  // Both snapshots are RGBA of the same size; compare every pixel.
  const auto left = planned.makePixmap();
  const auto right = plain.makePixmap();
  ASSERT_EQ(left.width(), right.width());
  ASSERT_EQ(left.height(), right.height());
  int mismatches = 0;
  for (int y = 0; y < left.height(); ++y) {
    for (int x = 0; x < left.width(); ++x) {
      if (left.getPixelARGB32(x, y) != right.getPixelARGB32(x, y)) ++mismatches;
    }
  }
  EXPECT_EQ(mismatches, 0) << "the plan-assembled frame differs from the canvas-only frame";
}

TEST(UiRetained, HitTestingUsesTheSameOrderAsPainting) {
  // Two overlapping children with the same z-index: the later one paints on top, so it must also be the
  // one a click lands on. A hand-written second sort in hitTest is how a container ends up painted in one
  // order and hit-tested in another.
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(40.0f, 40.0f));
  auto under = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 20, 30, 255));
  under->setBounds(Rect::MakeWH(40.0f, 40.0f));
  auto over = std::make_shared<ColorLayer>(Color::MakeRGBA8(200, 100, 50, 255));
  over->setBounds(Rect::MakeWH(40.0f, 40.0f));
  root->addChild(under);
  root->addChild(over);
  EXPECT_EQ(root->hitTest({ 20.0f, 20.0f }), over.get())
    << "the layer painted last is hit first";

  // zIndex reorders both: a higher z paints later and is hit first.
  under->setZIndex(5);
  EXPECT_EQ(root->paintOrderedChildren().back(), under.get());
  EXPECT_EQ(root->hitTest({ 20.0f, 20.0f }), under.get())
    << "the same order decides painting and hit testing";

  // And the order helper is what the paint path uses, so the two cannot drift silently.
  const std::vector<Layer*> ordered = root->paintOrderedChildren();
  ASSERT_EQ(ordered.size(), 2u);
  EXPECT_EQ(ordered.front(), over.get()) << "z ascending: the lower z first";
}

TEST(UiRetained, RecordingIntoAListPutsTheSubtreeAtItsOwnOrigin) {
  auto colour = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  colour->setBounds(Rect::MakeLTRB(10.0f, 20.0f, 30.0f, 40.0f));

  const auto list = colour->recordInto(Size{ 100.0f, 100.0f }, 1.0f);
  ASSERT_NE(list, nullptr) << "a recording canvas could not be created";
  ASSERT_FALSE(list->commands.empty());

  // The reference: the same rectangle drawn at the origin, in a canvas of the same size. Recording the
  // layer at its own bounds has to produce exactly that picture.
  auto reference = std::make_shared<graphics::gpu::DrawList>();
  std::unique_ptr<graphics::Canvas> canvas =
    graphics::Canvas::createRecordingBackend(100, 100, nullptr, reference);
  ASSERT_NE(canvas, nullptr);
  auto atOrigin = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  atOrigin->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 20.0f, 20.0f));
  Canvas uiCanvas = detail::makeCanvas(*canvas);
  RenderContext context(uiCanvas, 1.0f, nullptr, Size{ 100.0f, 100.0f }, nullptr);
  atOrigin->draw(context);
  ASSERT_FALSE(reference->commands.empty());

  const auto& recorded = list->commands.front().geometry[0];
  const auto& expected = reference->commands.front().geometry[0];
  EXPECT_FLOAT_EQ(recorded.getLeft(), expected.getLeft())
    << "the subtree's origin became the list's origin";
  EXPECT_FLOAT_EQ(recorded.getTop(), expected.getTop());
  EXPECT_FLOAT_EQ(recorded.getWidth(), expected.getWidth());
  EXPECT_FLOAT_EQ(recorded.getHeight(), expected.getHeight());
}

TEST(UiRetained, ACompositedViewportScrollsToTheSamePixelsAsTheLegacyPath) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  // A scroll viewport in a plan-mode frame lends its content at a fixed origin with the scroll offset as a
  // translation. The picture must not change when it does: same scroll, both paths, same pixels.
  auto build = [] {
    auto root = std::make_shared<Flex>(Axis::Column);
    root->setBounds(Rect::MakeWH(64.0f, 64.0f));
    auto view = std::make_shared<ScrollView>();
    LayoutParams grow;
    grow.grow = 1.0f;
    view->setLayoutParams(grow);
    view->setRepaintBoundary(true);
    auto content = std::make_shared<Flex>(Axis::Column);
    for (int index = 0; index < 6; ++index) {
      auto band = std::make_shared<ColorLayer>(
        Color::MakeRGBA8(static_cast<uint8_t>(40 + index * 30), static_cast<uint8_t>(200 - index * 20), 90, 255));
      LayoutParams row;
      row.basis = 16.0f;
      band->setLayoutParams(row);
      content->addChild(band);
    }
    view->setContent(content);
    root->addChild(view);
    return std::make_tuple(root, view);
  };

  std::vector<uint32_t> pixels[2];
  for (int pass = 0; pass < 2; ++pass) {
    auto [root, view] = build();
    Window::Options options;
    options.width = 64;
    options.height = 64;
    options.headless = true;
    auto window = Window::create(options);
    ASSERT_NE(window, nullptr) << "the headless window could not be created";
    window->setRoot(root);
    // The scope must cover the *first* frame too: it decides whether the viewport lays its content out
    // at the scrolled position (merged) or at a fixed origin (plan), and flipping it after that frame
    // leaves the rows where the other mode put them.
    Window::FramePlanScope planScope(pass == 0);
    ASSERT_TRUE(window->renderFrame());
    view->setScrollOffset(0.0f, 12.0f);
    // A full repaint each time: an idle frame takes the retained path, which reports no plan items and
    // would make the assertion below say the plan was not used.
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    if (pass == 0) EXPECT_GT(window->frameStatistics().framePlanItems, 0u) << "the plan path was used";
    graphics::Bitmap snapshot;
    ASSERT_TRUE(window->snapshot(&snapshot));
    ASSERT_FALSE(snapshot.isEmpty());
    const graphics::Pixmap pixmap = snapshot.makePixmap();
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) pixels[pass].push_back(pixmap.getPixelARGB32(x, y));
    }
  }
  for (std::size_t index = 0; index < pixels[0].size(); ++index) {
    ASSERT_EQ(pixels[0][index], pixels[1][index])
      << "the planned scroll differs at pixel " << index % 64 << ", " << index / 64;
  }
}

TEST(UiRetained, APlanModeFramePaintsTheOverlayAboveEveryBoundary) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  // The overlay is a second tree above the page, and a plan-mode frame submits the boundary lists after the
  // page content. If the overlay were flattened into the content list it would be painted *under* every
  // boundary, which is a popup covered by the panel it hangs over.
  auto build = [] {
    auto root = std::make_shared<Flex>(Axis::Column);
    root->setBounds(Rect::MakeWH(64.0f, 64.0f));
    auto panel = std::make_shared<Flex>(Axis::Column);
    panel->setRepaintBoundary(true);
    LayoutParams grow;
    grow.grow = 1.0f;
    panel->setLayoutParams(grow);
    auto fill = std::make_shared<ColorLayer>(Color::MakeRGBA8(200, 30, 30, 255));
    fill->setLayoutParams(grow);
    panel->addChild(fill);
    root->addChild(panel);
    return root;
  };

  std::vector<uint32_t> pixels[2];
  for (int pass = 0; pass < 2; ++pass) {
    auto root = build();
    Window::Options options;
    options.headless = true;
    options.width = 64;
    options.height = 64;
    auto window = Window::create(options);
    ASSERT_NE(window, nullptr);
    window->setRoot(root);

    // The overlay covers the top-left quarter, over the boundary panel.
    auto overlay = std::make_shared<Flex>();
    overlay->setBounds(Rect::MakeWH(64.0f, 64.0f));
    auto badge = std::make_shared<ColorLayer>(Color::MakeRGBA8(30, 200, 30, 255));
    badge->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 32.0f, 32.0f));
    overlay->addChild(badge);
    window->setOverlay(overlay);

    ASSERT_TRUE(window->renderFrame());
    ASSERT_TRUE(window->renderFrame());
    Window::FramePlanScope planScope(pass == 0);
    // A full repaint each time: an idle frame takes the retained path, which reports no plan items and
    // would make the assertion below say the plan was not used.
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    if (pass == 0) EXPECT_GT(window->frameStatistics().framePlanItems, 0u) << "the plan path was used";

    graphics::Bitmap snapshot;
    ASSERT_TRUE(window->snapshot(&snapshot));
    ASSERT_FALSE(snapshot.isEmpty());
    const graphics::Pixmap pixmap = snapshot.makePixmap();
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) pixels[pass].push_back(pixmap.getPixelARGB32(x, y));
    }
  }
  for (std::size_t index = 0; index < pixels[0].size(); ++index) {
    ASSERT_EQ(pixels[0][index], pixels[1][index])
      << "the planned frame differs at pixel " << index % 64 << ", " << index / 64
      << " planned=" << std::hex << pixels[0][index] << " legacy=" << pixels[1][index] << std::dec;
  }
}

TEST(UiRetained, TheFramePlanIsOnUnlessItIsTurnedOff) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  // The switch is process-wide, so this also pins the scoped helper: leaving a forced-off scope must put
  // the default back, or every later test would run a different frame path.
  EXPECT_EQ(Window::framePlanOverride(), -1) << "a previous test left the switch forced";
  {
    const Window::FramePlanScope planScope(false);
    EXPECT_EQ(Window::framePlanOverride(), 0);
  }
  EXPECT_EQ(Window::framePlanOverride(), -1) << "the scope did not restore the previous setting";

  auto root = std::make_shared<Flex>(Axis::Column);
  root->setBounds(Rect::MakeWH(32.0f, 32.0f));
  root->setCrossAlign(Align::Stretch);
  auto panel = std::make_shared<Flex>(Axis::Column);
  panel->setRepaintBoundary(true);
  LayoutParams grow;
  grow.grow = 1.0f;
  panel->setLayoutParams(grow);
  auto fill = std::make_shared<ColorLayer>(Color::MakeRGBA8(120, 60, 200, 255));
  fill->setLayoutParams(grow);
  panel->addChild(fill);
  root->addChild(panel);

  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  root->setNeedsPaint();
  ASSERT_TRUE(window->renderFrame());
  EXPECT_GT(window->frameStatistics().framePlanItems, 0u)
    << "the default frame is the plan: the tree has a boundary and the platform takes lists";
  {
    const Window::FramePlanScope merged(false);
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    EXPECT_EQ(window->frameStatistics().framePlanItems, 0u)
      << "the escape hatch still restores the merged frame";
  }
}

TEST(UiRetained, TheFrameStatisticsSayHowManyListsTheDeviceReceived) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  // `presentItems` / `compositeItems` / `lentListBytes` are the three metrics the project document promised
  // and did not have: how many lists reached the device, how many composite items the frame was assembled
  // from, and how much of it was borrowed rather than copied.
  auto root = std::make_shared<Flex>(Axis::Column);
  for (int index = 0; index < 2; ++index) {
    auto panel = std::make_shared<Flex>(Axis::Column);
    panel->setRepaintBoundary(true);
    auto fill = std::make_shared<ColorLayer>(Color::MakeRGBA8(static_cast<uint8_t>(60 + index * 80), 90, 160, 255));
    fill->setBounds(Rect::MakeWH(64.0f, 24.0f));
    panel->addChild(fill);
    root->addChild(panel);
  }

  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());

  {
    const Window::FramePlanScope planScope(false);
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    // The statistics describe the frame that finished, so the first frame under a different switch is
    // still the previous one's numbers.
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    const FrameStatistics plain = window->frameStatistics();
    EXPECT_EQ(plain.presentItems, 1u) << "a flattened frame is one list on the device";
    EXPECT_EQ(plain.lentListBytes, 0u) << "a flattened frame lent nothing";
    EXPECT_EQ(plain.compositeItems, 0u);
  }

  {
    const Window::FramePlanScope planScope(true);
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    const FrameStatistics planned = window->frameStatistics();
    // Each boundary lends its own paint and its child's, so two boundaries are four lists; the frame adds
    // the recorded content in front of them.
    EXPECT_EQ(planned.compositeItems, 2u) << "the two boundaries are the frame's composite items";
    EXPECT_EQ(planned.framePlanItems, 4u) << "one list per segment of each boundary";
    EXPECT_EQ(planned.presentItems, 5u) << "the content list plus the four lent ones";
    EXPECT_GT(planned.lentListBytes, 0u) << "the boundaries were borrowed, not copied";
    EXPECT_EQ(planned.frameListBytes, 0u) << "and nothing was copied into the frame";
  }
}

TEST(UiRetained, TheFrameStatisticsBelongToTheirWindow) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  // `framePlanItems` and friends live on the window, not on the process: two windows with different frame
  // policies must not read each other's numbers. A shared counter here would make a diagnostics overlay in
  // one window describe the other's frames.
  struct Built {
    std::shared_ptr<Window> window;
    std::shared_ptr<Layer> root;
  };
  const auto makeWindow = [](bool boundary) {
    Built built;
    auto root = std::make_shared<Flex>(Axis::Column);
    auto panel = std::make_shared<Flex>(Axis::Column);
    if (boundary) panel->setRepaintBoundary(true);
    auto fill = std::make_shared<ColorLayer>(Color::MakeRGBA8(70, 130, 190, 255));
    fill->setBounds(Rect::MakeWH(32.0f, 32.0f));
    panel->addChild(fill);
    root->addChild(panel);

    Window::Options options;
    options.headless = true;
    options.width = 32;
    options.height = 32;
    built.window = Window::create(options);
    if (built.window != nullptr) built.window->setRoot(root);
    built.root = root;
    return built;
  };

  const Built planned = makeWindow(true);
  const Built plain = makeWindow(false);
  const std::shared_ptr<Window>& plannedWindow = planned.window;
  const std::shared_ptr<Window>& plainWindow = plain.window;
  const std::shared_ptr<Layer>& plannedRoot = planned.root;
  const std::shared_ptr<Layer>& plainRoot = plain.root;
  ASSERT_NE(plannedWindow, nullptr);
  ASSERT_NE(plainWindow, nullptr);
  ASSERT_TRUE(plannedWindow->renderFrame());
  ASSERT_TRUE(plainWindow->renderFrame());

  const Window::FramePlanScope planScope(true);
  plannedRoot->setNeedsPaint();
  plainRoot->setNeedsPaint();
  ASSERT_TRUE(plannedWindow->renderFrame());
  ASSERT_TRUE(plainWindow->renderFrame());
  plannedRoot->setNeedsPaint();
  plainRoot->setNeedsPaint();
  ASSERT_TRUE(plannedWindow->renderFrame());
  ASSERT_TRUE(plainWindow->renderFrame());

  EXPECT_GT(plannedWindow->frameStatistics().framePlanItems, 0u)
    << "the window with a boundary planned its frame";
  EXPECT_EQ(plainWindow->frameStatistics().framePlanItems, 0u)
    << "the window without one did not, although the switch is on";
  EXPECT_GT(plannedWindow->frameStatistics().lentListBytes, 0u);
  EXPECT_EQ(plainWindow->frameStatistics().lentListBytes, 0u);
}

TEST(UiRetained, AThreadedWindowWithAnOverlayPaintsWhatTheSameThreadPathPaints) {
  // Three features meet here and nowhere else: the plan lends the boundary lists, the render thread takes
  // them through the packet, and the overlay has to end up above them. Each has its own test; the
  // combination is where the packet's list order and the overlay's "last item" rule actually interact.
  struct Built {
    std::unique_ptr<Window> window;
    std::shared_ptr<Layer> root;
  };
  const auto build = [](const RenderHostKind host) {
    Built built;
    auto root = std::make_shared<Flex>(Axis::Column);
    auto panel = std::make_shared<Flex>(Axis::Column);
    panel->setRepaintBoundary(true);
    auto fill = std::make_shared<ColorLayer>(Color::MakeRGBA8(200, 40, 40, 255));
    fill->setBounds(Rect::MakeWH(64.0f, 64.0f));
    panel->addChild(fill);
    root->addChild(panel);

    Window::Options options;
    options.headless = true;
    options.width = 64;
    options.height = 64;
    options.renderHost = host;
    built.window = Window::create(options);
    if (built.window != nullptr) built.window->setRoot(root);
    built.root = root;
    return built;
  };

  std::vector<uint32_t> pixels[2];
  for (int pass = 0; pass < 2; ++pass) {
    Built built = build(pass == 0 ? RenderHostKind::RenderThread : RenderHostKind::SameThread);
    ASSERT_NE(built.window, nullptr) << "the window could not be created";
    // The overlay covers the top-left quarter, over the boundary panel.
    auto overlay = std::make_shared<Flex>();
    overlay->setBounds(Rect::MakeWH(64.0f, 64.0f));
    auto badge = std::make_shared<ColorLayer>(Color::MakeRGBA8(30, 200, 30, 255));
    badge->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 32.0f, 32.0f));
    overlay->addChild(badge);
    built.window->setOverlay(overlay);

    const Window::FramePlanScope planScope(true);
    for (int frame = 0; frame < 3; ++frame) {
      built.root->setNeedsPaint();
      ASSERT_TRUE(built.window->renderFrame()) << built.window->diagnostics();
    }
    graphics::Bitmap snapshot;
    ASSERT_TRUE(built.window->snapshot(&snapshot)) << built.window->diagnostics();
    ASSERT_FALSE(snapshot.isEmpty());
    const graphics::Pixmap pixmap = snapshot.makePixmap();
    ASSERT_TRUE(pixmap.isValid());
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) pixels[pass].push_back(pixmap.getPixelARGB32(x, y));
    }
  }
  for (std::size_t index = 0; index < pixels[0].size(); ++index) {
    ASSERT_EQ(pixels[0][index], pixels[1][index])
      << "the threaded frame differs at pixel " << index % 64 << ", " << index / 64;
  }
}

TEST(UiRetained, AHighDpiPlanFramePaintsTheSamePixelsAsTheLegacyPath) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  // The plan's composite items carry a translation and a clip in *device* pixels, so the scale is where
  // those two can disagree with the canvas path. Every other pixel gate runs at 1.0. This one covers a
  // painted viewport at 1.5x with the offset at zero; the scrolled case is the DISABLED test below.
  struct Built {
    std::unique_ptr<Window> window;
    std::shared_ptr<Layer> root;
    std::shared_ptr<ScrollView> view;
  };
  const auto build = [] {
    Built built;
    auto root = std::make_shared<Flex>(Axis::Column);
    auto view = std::make_shared<ScrollView>();
    view->setRepaintBoundary(true);
    LayoutParams grow;
    grow.grow = 1.0f;
    view->setLayoutParams(grow);
    auto content = std::make_shared<Flex>(Axis::Column);
    for (int index = 0; index < 8; ++index) {
      auto band = std::make_shared<ColorLayer>(
        Color::MakeRGBA8(static_cast<uint8_t>(40 + index * 25), static_cast<uint8_t>(200 - index * 15), 90, 255));
      band->setBounds(Rect::MakeWH(64.0f, 16.0f));
      content->addChild(band);
    }
    view->setContent(content);
    root->addChild(view);

    Window::Options options;
    options.headless = true;
    options.width = 96;
    options.height = 96;
    options.uiScale = 1.5f;
    built.window = Window::create(options);
    if (built.window != nullptr) built.window->setRoot(root);
    built.root = root;
    built.view = view;
    return built;
  };

  std::vector<uint32_t> pixels[2];
  for (int pass = 0; pass < 2; ++pass) {
    Built built = build();
    ASSERT_NE(built.window, nullptr);
    ASSERT_FLOAT_EQ(built.window->uiScale(), 1.5f) << "the window did not take the override";
    const Window::FramePlanScope planScope(pass == 0);
    ASSERT_TRUE(built.window->renderFrame());
    built.view->setScrollOffset(0.0f, 0.0f);
    ASSERT_TRUE(built.window->renderFrame());
    built.root->setNeedsPaint();
    ASSERT_TRUE(built.window->renderFrame());
    if (pass == 0) EXPECT_GT(built.window->frameStatistics().framePlanItems, 0u) << "the plan ran";

    graphics::Bitmap snapshot;
    ASSERT_TRUE(built.window->snapshot(&snapshot)) << built.window->diagnostics();
    ASSERT_FALSE(snapshot.isEmpty());
    const graphics::Pixmap pixmap = snapshot.makePixmap();
    ASSERT_TRUE(pixmap.isValid());
    for (int y = 0; y < pixmap.height(); ++y) {
      for (int x = 0; x < pixmap.width(); ++x) pixels[pass].push_back(pixmap.getPixelARGB32(x, y));
    }
  }
  ASSERT_EQ(pixels[0].size(), pixels[1].size());
  for (std::size_t index = 0; index < pixels[0].size(); ++index) {
    ASSERT_EQ(pixels[0][index], pixels[1][index])
      << "the high-DPI plan frame differs at pixel " << index % 96 << ", " << index / 96
      << " planned=" << std::hex << pixels[0][index] << " legacy=" << pixels[1][index] << std::dec;
  }
}

/**
 * A scrolled viewport at 1.5x through the plan path does **not** match the canvas path, and this test is
 * disabled rather than deleted so the defect stays reproducible in one command.
 *
 * Observed on 2026-11-18: at pixel (0, 6) the plan frame paints band 1 (`ff41b95a`) where the canvas path
 * paints band 0 (`ff28c85a`) -- the content is a whole band (24 device pixels) out of place -- while the
 * same comparison at scale 1.0 matches (`ACompositedViewportScrollsToTheSamePixelsAsTheLegacyPath`) and the
 * unscrolled 1.5x frame matches (`AHighDpiPlanFramePaintsTheSamePixels…`). So it is the combination of a
 * non-zero scroll offset and a scale above 1.
 *
 * Instrumented since: the viewport reports `vbounds=(0,0,64,64)`, `scroll=6`, content bounds `(0,0,64,128)`
 * and a content-item translation of `(0,-9)`, with the content list's own bounds `(0,-9,96,183)` -- i.e. the
 * numbers are the ones the picture needs, so the shift is applied somewhere between that item and the
 * device. Two device-space details are the remaining suspects: the composite item's translation goes into
 * the canvas block (which is shared by every list of the frame) and the clip is a scissor; at 1.5x a
 * one-pixel scissor rounding moves a band's edge by more than a device pixel.
 */
TEST(UiRetained, AHighDpiScrolledPlanFramePaintsTheSamePixelsAsTheLegacyPath) {
  struct Built {
    std::unique_ptr<Window> window;
    std::shared_ptr<Layer> root;
    std::shared_ptr<ScrollView> view;
  };
  const auto build = [] {
    Built built;
    auto root = std::make_shared<Flex>(Axis::Column);
    auto view = std::make_shared<ScrollView>();
    view->setRepaintBoundary(true);
    LayoutParams grow;
    grow.grow = 1.0f;
    view->setLayoutParams(grow);
    auto content = std::make_shared<Flex>(Axis::Column);
    for (int index = 0; index < 8; ++index) {
      auto band = std::make_shared<ColorLayer>(
        Color::MakeRGBA8(static_cast<uint8_t>(40 + index * 25), static_cast<uint8_t>(200 - index * 15), 90, 255));
      band->setBounds(Rect::MakeWH(64.0f, 16.0f));
      content->addChild(band);
    }
    view->setContent(content);
    root->addChild(view);

    Window::Options options;
    options.headless = true;
    options.width = 96;
    options.height = 96;
    options.uiScale = 1.5f;
    built.window = Window::create(options);
    if (built.window != nullptr) built.window->setRoot(root);
    built.root = root;
    built.view = view;
    return built;
  };

  std::vector<uint32_t> pixels[2];
  for (int pass = 0; pass < 2; ++pass) {
    Built built = build();
    ASSERT_NE(built.window, nullptr);
    // The mode has to be chosen before the first frame: it decides whether the content is laid out at
    // the scrolled position or at a fixed origin translated per frame.
    const Window::FramePlanScope planScope(pass == 0);
    ASSERT_TRUE(built.window->renderFrame());
    built.view->setScrollOffset(0.0f, 6.0f);
    ASSERT_TRUE(built.window->renderFrame());
    built.root->setNeedsPaint();
    ASSERT_TRUE(built.window->renderFrame());
    graphics::Bitmap snapshot;
    ASSERT_TRUE(built.window->snapshot(&snapshot)) << built.window->diagnostics();
    const graphics::Pixmap pixmap = snapshot.makePixmap();
    ASSERT_TRUE(pixmap.isValid());
    for (int y = 0; y < pixmap.height(); ++y) {
      for (int x = 0; x < pixmap.width(); ++x) pixels[pass].push_back(pixmap.getPixelARGB32(x, y));
    }
  }
  std::size_t firstBad = pixels[0].size();
  int reported = 0;
  for (std::size_t index = 0; index < pixels[0].size(); ++index) {
    if (pixels[0][index] == pixels[1][index]) continue;
    if (firstBad == pixels[0].size()) firstBad = index;
    ++reported;
  }
  ASSERT_EQ(reported, 0)
    << "the scrolled high-DPI plan frame differs in " << reported << " pixels, first at "
    << firstBad % 96 << ", " << firstBad / 96;
}

TEST(UiRetained, AHighDpiScrolledViewportWithTwoBandsMatchesWithoutTheScrollbar) {
  // The narrowest version of the high-DPI scrolled difference: one viewport, two bands, and no scrollbar
  // drawn (the thumb colour is transparent, so nothing but the content is in the frame). If this matches
  // the canvas path, the difference belongs to the scrollbar item; if it does not, it belongs to the
  // content item's place in the order.
  struct Built {
    std::unique_ptr<Window> window;
    std::shared_ptr<Layer> root;
    std::shared_ptr<ScrollView> view;
  };
  const auto build = [] {
    Built built;
    auto root = std::make_shared<Flex>(Axis::Column);
    auto view = std::make_shared<ScrollView>();
    view->setRepaintBoundary(true);
    LayoutParams grow;
    grow.grow = 1.0f;
    view->setLayoutParams(grow);
    auto content = std::make_shared<Flex>(Axis::Column);
    for (int index = 0; index < 4; ++index) {
      auto band = std::make_shared<ColorLayer>(
        Color::MakeRGBA8(static_cast<uint8_t>(40 + index * 50), static_cast<uint8_t>(200 - index * 40), 90, 255));
      band->setBounds(Rect::MakeWH(64.0f, 24.0f));
      content->addChild(band);
    }
    view->setContent(content);
    root->addChild(view);

    Window::Options options;
    options.headless = true;
    options.width = 96;
    options.height = 96;
    options.uiScale = 1.5f;
    built.window = Window::create(options);
    if (built.window != nullptr) built.window->setRoot(root);
    built.root = root;
    built.view = view;
    return built;
  };

  std::vector<uint32_t> pixels[2];
  for (int pass = 0; pass < 2; ++pass) {
    Built built = build();
    ASSERT_NE(built.window, nullptr);
    // The mode decides where the first layout puts the rows, so it must be chosen before the first frame.
    const Window::FramePlanScope planScope(pass == 0);
    ASSERT_TRUE(built.window->renderFrame());
    built.view->setScrollOffset(0.0f, 6.0f);
    ASSERT_TRUE(built.window->renderFrame());
    built.root->setNeedsPaint();
    ASSERT_TRUE(built.window->renderFrame());

    graphics::Bitmap snapshot;
    ASSERT_TRUE(built.window->snapshot(&snapshot)) << built.window->diagnostics();
    const graphics::Pixmap pixmap = snapshot.makePixmap();
    ASSERT_TRUE(pixmap.isValid());
    for (int y = 0; y < pixmap.height(); ++y) {
      for (int x = 0; x < pixmap.width(); ++x) pixels[pass].push_back(pixmap.getPixelARGB32(x, y));
    }
  }
  for (std::size_t index = 0; index < pixels[0].size(); ++index) {
    ASSERT_EQ(pixels[0][index], pixels[1][index])
      << "differs at pixel " << index % 96 << ", " << index / 96 << " planned=" << std::hex
      << pixels[0][index] << " legacy=" << pixels[1][index] << std::dec;
  }
}
namespace {

/** Puts the global culling switch back whatever way the test ends. */
class CullingScope {
 public:
  explicit CullingScope(const bool enabled) : mPrevious(Layer::drawCullingEnabled()) {
    Layer::setDrawCullingEnabled(enabled);
  }
  ~CullingScope() { Layer::setDrawCullingEnabled(mPrevious); }
  CullingScope(const CullingScope&) = delete;
  CullingScope& operator=(const CullingScope&) = delete;

 private:
  bool mPrevious;
};

} // namespace

TEST(UiRetained, ScrollViewCullsOffscreenRowsAndKeepsThePixels) {
  // A viewport tall enough for a handful of 16px bands: the rows above and below it must not be
  // recorded, and the picture must not change because of that.
  auto build = [] {
    auto root = std::make_shared<Flex>(Axis::Column);
    root->setCrossAlign(Align::Stretch);
    auto view = std::make_shared<ScrollView>();
    LayoutParams grow;
    grow.grow = 1.0f;
    view->setLayoutParams(grow);
    auto content = std::make_shared<Flex>(Axis::Column);
    for (int index = 0; index < 60; ++index) {
      auto band = std::make_shared<ColorLayer>(Color::MakeRGBA8(
        static_cast<uint8_t>(30 + index * 3), static_cast<uint8_t>(200 - index * 2), 90, 255));
      LayoutParams row;
      row.basis = 16.0f;
      band->setLayoutParams(row);
      content->addChild(band);
    }
    view->setContent(content);
    root->addChild(view);
    return std::make_tuple(root, view);
  };

  std::vector<uint32_t> pixels[2];
  uint32_t culled = 0;
  for (int pass = 0; pass < 2; ++pass) {
    auto [root, view] = build();
    Window::Options options;
    options.width = 96;
    options.height = 64;
    options.headless = true;
    auto window = Window::create(options);
    ASSERT_NE(window, nullptr) << "the headless window could not be created";
    window->setRoot(root);
    ASSERT_TRUE(window->renderFrame());
    view->setScrollOffset(0.0f, 160.0f);
    // Pass 0 culls, pass 1 draws everything; the captures must match.
    CullingScope culling(pass == 0);
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    root->setNeedsPaint();
    ASSERT_TRUE(window->renderFrame());
    if (pass == 0) culled = window->frameStatistics().culledLayers;
    graphics::Bitmap snapshot;
    ASSERT_TRUE(window->snapshot(&snapshot));
    ASSERT_FALSE(snapshot.isEmpty());
    const graphics::Pixmap pixmap = snapshot.makePixmap();
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 96; ++x) pixels[pass].push_back(pixmap.getPixelARGB32(x, y));
    }
  }
  EXPECT_GT(culled, 10u) << "the rows outside the viewport have to be skipped";
  for (std::size_t index = 0; index < pixels[0].size(); ++index) {
    ASSERT_EQ(pixels[0][index], pixels[1][index])
      << "culling changed the picture at " << index % 96 << ", " << index / 96;
  }
}

TEST(UiRetained, AFlexAssignedViewportAbsorbsLayoutBelowIt) {
  // The viewport's parent is a Row that gives it a grow share and stretches its height, so both axes
  // are the parent's decision; a text change inside the content must then stay inside the viewport.
  auto root = std::make_shared<Flex>(Axis::Row);
  root->setCrossAlign(Align::Stretch);
  auto view = std::make_shared<ScrollView>();
  LayoutParams grow;
  grow.grow = 1.0f;
  grow.basis = 0.0f;
  view->setLayoutParams(grow);
  auto content = std::make_shared<Flex>(Axis::Column);
  const auto font = graphics::Font::create(
    graphics::Typeface::fromFile(std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf"), 14.0f);
  if (font == nullptr) GTEST_SKIP() << "the UI test font is not available";
  auto label = std::make_shared<TextLayer>("short", font);
  content->addChild(label);
  view->setContent(content);
  root->addChild(view);

  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 200;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->renderFrame());
  EXPECT_TRUE(view->parentAssignedSize())
    << "a grow share plus a stretched cross axis is the parent assigning both axes";

  label->setText("a label long enough to want more room than the short one needed");
  EXPECT_FALSE(root->needsLayout()) << "the viewport is a layout boundary now";
  EXPECT_TRUE(view->subtreeNeedsLayout()) << "but the pass still reaches it";
  ASSERT_TRUE(window->renderFrame());
  EXPECT_LE(window->frameStatistics().layoutDirtyNodes, 4u);
  EXPECT_FALSE(root->needsLayout());
}

namespace {

/** The colour a virtual or plain row of a 10000-row list has, so the two can be compared. */
constexpr uint32_t rowArgb(const int index) {
  const auto red = static_cast<uint32_t>(index % 256);
  const auto green = static_cast<uint32_t>((index / 256) % 256);
  return 0xFF000000u | (red << 16) | (green << 8) | 200u;
}

/**
 * A 10000-row content that only materializes the rows near the viewport.
 *
 * The pool is the whole point: the test asserts it stays the size of a few viewports while the scroll
 * offset walks the list, which is what "10000 rows cost the same as 240" means in nodes and memory.
 */
class RowProvider final : public ScrollView::VirtualContent {
 public:
  RowProvider(const int rows, const float rowHeight) : mRows(rows), mRowHeight(rowHeight) {}

  [[nodiscard]] Size contentSize(const Size& viewport) override {
    return Size{ std::max(viewport.width, 64.0f), static_cast<float>(mRows) * mRowHeight };
  }

  void realize(Layer& host, const float top, const float bottom) override {
    const int first = std::max(0, static_cast<int>(top / mRowHeight));
    const int last = std::min(mRows - 1, static_cast<int>((bottom - 1.0f) / mRowHeight));
    const std::size_t needed = static_cast<std::size_t>(std::max(0, last - first + 1));
    while (mPool.size() < needed) {
      auto row = std::make_shared<ColorLayer>(Color::MakeRGBA8(0, 0, 0, 255));
      host.addChild(row);
      mPool.push_back(row);
    }
    for (std::size_t index = 0; index < mPool.size(); ++index) {
      const bool used = index < needed;
      mPool[index]->setVisible(used);
      if (!used) continue;
      const int rowIndex = first + static_cast<int>(index);
      const uint32_t argb = rowArgb(rowIndex);
      mPool[index]->setColor(Color::MakeRGBA8(static_cast<uint8_t>((argb >> 16) & 0xFF),
        static_cast<uint8_t>((argb >> 8) & 0xFF), static_cast<uint8_t>(argb & 0xFF),
        static_cast<uint8_t>((argb >> 24) & 0xFF)));
      mPool[index]->setBounds(Rect::MakeXYWH(host.bounds().getLeft(),
        host.bounds().getTop() + static_cast<float>(rowIndex) * mRowHeight,
        std::max(host.bounds().getWidth(), 64.0f), mRowHeight));
    }
  }

  void detach(Layer& host) override {
    for (const std::shared_ptr<ColorLayer>& row: mPool) {
      host.removeChild(std::static_pointer_cast<Layer>(row));
    }
    mPool.clear();
  }

  [[nodiscard]] std::size_t poolSize() const noexcept {
    return mPool.size();
  }

 private:
  int mRows = 0;
  float mRowHeight = 16.0f;
  std::vector<std::shared_ptr<ColorLayer>> mPool;
};

} // namespace

TEST(UiRetained, AVirtualViewportKeepsOnlyTheRowsNearTheViewport) {
  auto root = std::make_shared<Flex>(Axis::Column);
  root->setCrossAlign(Align::Stretch);
  auto view = std::make_shared<ScrollView>();
  LayoutParams grow;
  grow.grow = 1.0f;
  view->setLayoutParams(grow);
  auto provider = std::make_shared<RowProvider>(10000, 16.0f);
  view->setVirtualContent(provider);
  root->addChild(view);

  Window::Options options;
  options.headless = true;
  options.width = 96;
  options.height = 64;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  window->setRoot(root);
  const Window::FramePlanScope planScope(true);
  ASSERT_TRUE(window->renderFrame());
  // A 64px viewport shows four 16px rows and keeps one viewport of slack on each side.
  EXPECT_LE(provider->poolSize(), 16u) << "a virtual list must not materialize 10000 rows";
  EXPECT_GE(provider->poolSize(), 4u) << "the visible rows have to be there";

  view->setScrollOffset(0.0f, 5000.0f * 16.0f + 8.0f);
  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->renderFrame());
  EXPECT_LE(provider->poolSize(), 16u) << "scrolling recycles the pool instead of growing it";
  graphics::Bitmap snapshot;
  ASSERT_TRUE(window->snapshot(&snapshot));
  const graphics::Pixmap pixmap = snapshot.makePixmap();
  // The first visible pixel row is 8px into row 5000, so that pixel has to be row 5000's colour.
  EXPECT_EQ(pixmap.getPixelARGB32(48, 2), rowArgb(5000))
    << "the realized rows are not the ones at the scroll offset";
}

TEST(UiRetained, ACachedContainerSkipsTheSegmentsOutsideTheClip) {
  // A container caches one list per child and replays all of them. A child the clip cannot see
  // contributes nothing, so the replay has to skip it -- the segment counterpart of the draw pass's
  // cull. Ten bands in a 64px-tall clipped frame: two are visible, eight must not be replayed.
  auto build = [] {
    auto root = std::make_shared<Flex>(Axis::Column);
    root->setCrossAlign(Align::Stretch);
    auto frame = std::make_shared<Layer>();
    frame->setClip(true);
    LayoutParams grow;
    grow.grow = 1.0f;
    frame->setLayoutParams(grow);
    auto panel = std::make_shared<Layer>();
    // A boundary keeps the panel out of the frame's own recording: it replays its own segments, which
    // is the path this test is about.
    panel->setRepaintBoundary(true);
    panel->setBounds(Rect::MakeWH(96.0f, 400.0f));
    for (int index = 0; index < 10; ++index) {
      auto band = std::make_shared<ColorLayer>(Color::MakeRGBA8(
        static_cast<uint8_t>(60 + index * 15), 120, 200, 255));
      band->setBounds(Rect::MakeXYWH(0.0f, static_cast<float>(index) * 40.0f, 96.0f, 40.0f));
      panel->addChild(band);
    }
    frame->addChild(panel);
    root->addChild(frame);
    return std::make_tuple(root, panel);
  };

  auto [root, panel] = build();
  Window::Options options;
  options.headless = true;
  options.width = 96;
  options.height = 64;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  // The merged path, so the panel's own cached replay runs; with the plan on, the frame would lend the
  // panel's recording instead and the segment loop would not be the code under test.
  const Window::FramePlanScope planScope(false);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  // The first frame recorded the panel; the second replays its segments under the same clip.
  root->setNeedsPaint();
  ASSERT_TRUE(window->renderFrame());
  EXPECT_GE(window->frameStatistics().culledLayers, 8u)
    << "the replay appended segments the clip cannot see";
  graphics::Bitmap snapshot;
  ASSERT_TRUE(window->snapshot(&snapshot));
  const graphics::Pixmap pixmap = snapshot.makePixmap();
  EXPECT_EQ(pixmap.getPixelARGB32(10, 10), 0xFF3C78C8u) << "the first visible band is missing";
  EXPECT_EQ(pixmap.getPixelARGB32(10, 50), 0xFF4B78C8u) << "the second visible band is missing";
}