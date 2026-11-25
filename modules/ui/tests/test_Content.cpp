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
#include <ui/ContentHost.h>
#include <ui/ContentFrameBackend.h>
#include <ui/ContentView.h>
#include <ui/Flex.h>
#include <ui/RenderContext.h>
#include <ui/TextLayer.h>
#include <ui/Window.h>
#include <ui/WindowBackend.h>
#include <ui/CanvasBackend.h>

#include <graphics/Bitmap.h>
#include <graphics/Canvas.h>
#include <graphics/Paint.h>
#include <graphics/Typeface.h>

#include <gpu/Surface.h>

#include <gtest/gtest.h>

#include <vector>

namespace {

using namespace coral::ui;

ContentFrame frameOfSize(const uint32_t width, const uint32_t height) {
  graphics::ExternalImage image;
  image.texture = backend::TextureHandle{ 1, 1 };
  image.width = width;
  image.height = height;
  image.format = backend::PixelFormat::R8G8B8A8_UNORM;
  return detail::makeContentFrame(image);
}

TEST(UiContent, AThreadedContextRecordsTheFenceInsteadOfWaiting) {
  auto host = std::make_shared<ContentHost>();
  bool fenceRan = false;
  host->publish(frameOfSize(4, 4), [&] { fenceRan = true; });
  ContentView view(host);
  view.setBounds(Rect::MakeWH(8.0f, 8.0f));

  graphics::Bitmap target = graphics::Bitmap::allocate(8, 8);
  ASSERT_FALSE(target.isEmpty());
  auto canvas = graphics::Canvas::create(target);
  ASSERT_NE(canvas, nullptr);
  Canvas uiCanvas = detail::makeCanvas(*canvas);
  RenderContext context(uiCanvas, 1.0f);
  std::vector<ContentFrame> frames;
  std::vector<ContentHost::FrameFence> fences;
  context.setContentSink(&frames, &fences);
  view.draw(context);

  EXPECT_FALSE(fenceRan) << "the UI thread must not wait for the producer fence";
  ASSERT_EQ(frames.size(), 1u);
  ASSERT_EQ(fences.size(), 1u);
  fences.front()();
  EXPECT_TRUE(fenceRan);
}

TEST(UiContent, APublishedFrameIsTheLatestFrame) {
  ContentHost host;
  EXPECT_FALSE(host.latest().valid());
  EXPECT_EQ(host.generation(), 0u);

  const ContentFrame image = frameOfSize(640, 360);
  host.publish(image);
  EXPECT_EQ(host.generation(), 1u);
  EXPECT_EQ(host.latest().width(), 640u);
  EXPECT_EQ(host.latest().height(), 360u);

  host.clear();
  EXPECT_FALSE(host.latest().valid());
  EXPECT_EQ(host.generation(), 2u);
}

TEST(UiContent, APublishNotifiesTheFrameAvailableCallback) {
  ContentHost host;
  int calls = 0;
  uint64_t seenGeneration = 0;
  host.setFrameAvailableCallback([&] {
    ++calls;
    seenGeneration = host.generation();
  });

  const ContentFrame image = frameOfSize(4, 4);
  host.publish(image);
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(seenGeneration, 1u) << "the callback runs after the frame is recorded and unlocked";

  host.clear();
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(seenGeneration, 2u);
}

TEST(UiContent, HidingAViewMarksItsHostInactive) {
  auto host = std::make_shared<ContentHost>();
  ContentView view(host);
  EXPECT_TRUE(host->active()) << "a host starts active so a view that is never hidden works";

  view.setVisible(false);
  EXPECT_FALSE(host->active()) << "a hidden preview must be able to stop its producer";

  view.setVisible(true);
  EXPECT_TRUE(host->active());
}

TEST(UiContent, ASharedBudgetCapsMultiplePreviews) {
  auto budget = std::make_shared<ContentFrameBudget>(2);
  std::vector<std::shared_ptr<ContentHost>> hosts;
  for (int index = 0; index < 3; ++index) {
    hosts.push_back(std::make_shared<ContentHost>());
    hosts.back()->setFrameBudget(budget);
  }

  const auto first = hosts[0]->acquireSlot();
  const auto second = hosts[1]->acquireSlot();
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(budget->inUse(), 2u);
  EXPECT_FALSE(hosts[2]->acquireSlot().has_value())
    << "three previews must not reserve more than the shared two-slot pool";

  hosts[0]->releaseSlot(*first);
  EXPECT_EQ(budget->inUse(), 1u);
  EXPECT_TRUE(hosts[2]->acquireSlot().has_value()) << "the freed slot is reusable by another preview";
}

TEST(UiContent, HidingAViewReleasesItsBudgetSlot) {
  auto budget = std::make_shared<ContentFrameBudget>(1);
  auto host = std::make_shared<ContentHost>();
  host->setFrameBudget(budget);
  ContentView view(host);
  const auto slot = host->acquireSlot();
  ASSERT_TRUE(slot.has_value());
  EXPECT_EQ(budget->inUse(), 1u);

  view.setVisible(false);
  EXPECT_EQ(budget->inUse(), 0u) << "a hidden preview must not keep the frame pool";
  EXPECT_FALSE(host->active());
}

TEST(UiContent, DeviceRecoveryInvalidatesTheFrameAndNotifiesTheProducer) {
  ContentHost host;
  const ContentFrame image = frameOfSize(8, 8);
  host.publish(image);
  int recovered = 0;
  int available = 0;
  host.setRecoveryCallback([&] { ++recovered; });
  host.setFrameAvailableCallback([&] { ++available; });

  host.notifyDeviceRecovered();
  EXPECT_FALSE(host.latest().valid());
  EXPECT_EQ(recovered, 1);
  EXPECT_EQ(available, 1) << "the view still needs a repaint to show its placeholder";
  EXPECT_EQ(host.generation(), 2u);
}

TEST(UiContent, ActiveChangesAreReportedToTheProducer) {
  ContentHost host;
  std::vector<bool> states;
  host.setActiveChangedCallback([&](const bool active) { states.push_back(active); });

  host.setActive(false);
  host.setActive(false);
  host.setActive(true);
  ASSERT_EQ(states.size(), 2u);
  EXPECT_FALSE(states[0]);
  EXPECT_TRUE(states[1]);
}

TEST(UiContent, ResizeStretchesTheOldFrameUntilTheNewSizeArrives) {
  auto host = std::make_shared<ContentHost>();
  ContentView view(host);
  view.setBounds(Rect::MakeWH(8.0f, 8.0f));

  graphics::Bitmap target = graphics::Bitmap::allocate(32, 32);
  ASSERT_FALSE(target.isEmpty());
  auto canvas = graphics::Canvas::create(target);
  ASSERT_NE(canvas, nullptr);
  Canvas uiCanvas = detail::makeCanvas(*canvas);
  RenderContext context(uiCanvas, 1.0f);

  const ContentFrame oldFrame = frameOfSize(4, 4);
  host->publish(oldFrame);
  view.draw(context);
  EXPECT_TRUE(view.awaitingResize()) << "the 4x4 frame is stretched over the requested 8x8 preview";

  const ContentFrame newFrame = frameOfSize(8, 8);
  host->publish(newFrame);
  view.draw(context);
  EXPECT_FALSE(view.awaitingResize()) << "a matching frame ends the transition";
  EXPECT_FLOAT_EQ(view.frameSize().width, 8.0f);
}

TEST(UiContent, AViewRequestsADeviceSizedTarget) {
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  options.uiScale = 2.0f;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";

  auto host = std::make_shared<ContentHost>();
  auto content = std::make_shared<ContentView>(host);
  content->setBounds(Rect::MakeLTRB(4.0f, 4.0f, 12.0f, 12.0f));
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 0, 255));
  root->addChild(content);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  const Size requested = host->requestedSize();
  EXPECT_FLOAT_EQ(requested.width, 16.0f) << "logical 8 wide at uiScale 2 is a 16 px target";
  EXPECT_FLOAT_EQ(requested.height, 16.0f);
}

TEST(UiContent, AContentViewWithNoFrameDrawsItsPlaceholder) {
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 16;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto host = std::make_shared<ContentHost>();
  auto content = std::make_shared<ContentView>(host);
  content->setBounds(Rect::MakeWH(16.0f, 16.0f));
  content->setPlaceholderColor(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  window->setRoot(content);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixmap = bitmap.makePixmap();
  ASSERT_TRUE(pixmap.isValid());
  const uint8_t* pixel = static_cast<const uint8_t*>(pixmap.pixels());
  EXPECT_EQ(pixel[0], 0);
  EXPECT_EQ(pixel[1], 0);
  EXPECT_EQ(pixel[2], 255);
}

namespace {

/** Renders one opaque red frame into a producer surface owned by the window's device. */
std::unique_ptr<graphics::gpu::Surface> makeRedProducer(Window& window, const uint32_t width,
    const uint32_t height) {
  graphics::gpu::SurfaceConfiguration configuration;
  configuration.width = width;
  configuration.height = height;
  configuration.device = detail::deviceOf(window);
  configuration.label = "ui-test-content-producer";
  auto producer = graphics::gpu::Surface::make(configuration);
  if (producer == nullptr) return nullptr;

  graphics::Canvas* canvas = producer->beginFrame();
  if (canvas == nullptr) return nullptr;
  graphics::Paint red;
  red.color = graphics::Color::MakeRGBA8(255, 0, 0, 255);
  red.style = graphics::Paint::Style::kFill;
  canvas->clear(red.color);
  if (!producer->submit() || !producer->waitIdle()) return nullptr;
  return producer;
}

ContentFrame redImage(const graphics::gpu::Surface& producer) {
  graphics::ExternalImage image;
  image.texture = producer.target();
  image.width = producer.width();
  image.height = producer.height();
  image.format = backend::PixelFormat::R8G8B8A8_UNORM;
  image.premultiplied = false;
  return detail::makeContentFrame(image);
}

} // namespace

TEST(UiContent, AWindowSamplesThePublishedTextureOnItsOwnDevice) {
  // The regression this covers: `Window` used to build its RenderContext without a frame target, so
  // `ContentView` could only draw its placeholder even though the producer was publishing real frames.
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  if (detail::deviceOf(*window) == nullptr) GTEST_SKIP() << "the window has no device to share";

  const auto producer = makeRedProducer(*window, 32, 32);
  if (producer == nullptr) GTEST_SKIP() << "a second surface could not be created on the window device";

  auto host = std::make_shared<ContentHost>();
  host->publish(redImage(*producer));
  auto content = std::make_shared<ContentView>(host);
  content->setBounds(Rect::MakeWH(32.0f, 32.0f));
  content->setPlaceholderColor(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  window->setRoot(content);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixmap = bitmap.makePixmap();
  ASSERT_TRUE(pixmap.isValid());
  const uint32_t centre = pixmap.getPixelARGB32(16, 16);
  EXPECT_EQ((centre >> 16) & 0xFFu, 255u) << "the content must be the published red, not the placeholder blue";
  EXPECT_EQ((centre >> 8) & 0xFFu, 0u);
  EXPECT_EQ(centre & 0xFFu, 0u);
}

TEST(UiContent, AWindowAppliesItsUiScaleToTheContentRect) {
  // The external-image command is recorded through the canvas, so the root's logical-to-device scale is
  // part of the quad. This is what a DPR of 2 needs: logical bounds 8..16 must land on device 16..32.
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  options.uiScale = 2.0f;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  if (detail::deviceOf(*window) == nullptr) GTEST_SKIP() << "the window has no device to share";

  const auto producer = makeRedProducer(*window, 32, 32);
  if (producer == nullptr) GTEST_SKIP() << "a second surface could not be created on the window device";

  auto host = std::make_shared<ContentHost>();
  host->publish(redImage(*producer));
  auto content = std::make_shared<ContentView>(host);
  content->setBounds(Rect::MakeLTRB(8.0f, 8.0f, 16.0f, 16.0f));
  // The root is stretched to the window, so the content has to be a child for its own bounds to survive.
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 0, 255));
  root->addChild(content);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixmap = bitmap.makePixmap();
  ASSERT_TRUE(pixmap.isValid());
  const uint32_t inside = pixmap.getPixelARGB32(20, 20);
  EXPECT_EQ((inside >> 16) & 0xFFu, 255u) << "the logical rect must be scaled into device pixels";
  EXPECT_EQ(pixmap.getPixelARGB32(4, 4) & 0x00FFFFFFu, 0u) << "outside the content rect stays cleared";
}

TEST(UiContent, ADeviceRecoveryDropsTheOldTexture) {
  auto host = std::make_shared<ContentHost>();
  ContentView view(host);
  ContentFrame image;
  host->publish(image);
  const uint64_t before = host->generation();
  view.onDeviceRecovered();
  EXPECT_FALSE(host->latest().valid()) << "a texture from the lost device must not be sampled";
  EXPECT_GT(host->generation(), before) << "clearing publishes a visible change to the view";
}

TEST(UiContent, AFrameFenceRunsBeforeConsumption) {
  ContentHost host;
  int waits = 0;
  ContentFrame image;
  host.publish(image, [&] { ++waits; });
  EXPECT_EQ(waits, 0) << "publishing must not block";
  host.waitForLatestFrame();
  EXPECT_EQ(waits, 1);
  host.clear();
  host.waitForLatestFrame();
  EXPECT_EQ(waits, 1) << "a cleared host has no fence";
}

TEST(UiContent, InFlightSlotsCapAProducer) {
  ContentHost host;
  host.setMaxInFlight(2);
  ASSERT_EQ(host.maxInFlight(), 2u);
  auto first = host.acquireSlot();
  auto second = host.acquireSlot();
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_FALSE(host.acquireSlot().has_value()) << "the producer must back off instead of queueing";
  EXPECT_EQ(host.inFlight(), 2u);

  ContentFrame image;
  host.publish(*first, image);
  EXPECT_EQ(host.inFlight(), 2u) << "a published frame still owns its slot";
  host.waitForLatestFrame();
  EXPECT_EQ(host.inFlight(), 1u) << "consuming the latest frame releases its slot";
  host.releaseSlot(*second);
  EXPECT_EQ(host.inFlight(), 0u);
  EXPECT_TRUE(host.acquireSlot().has_value());
}

} // namespace

TEST(UiContent, TextLayersRemainVisibleOverATransparentClear) {
  Window::Options options;
  options.headless = true;
  options.width = 240;
  options.height = 80;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 1);
  options.frameLimit = 1;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  const auto typeface = graphics::Typeface::fromFile(
    std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf");
  ASSERT_NE(typeface, nullptr);
  auto text = std::make_shared<TextLayer>("Transparent text",
    graphics::Font::create(typeface, 24.0f), graphics::Color::White());
  text->setFontFeatures({ graphics::FontFeature{ .tag = "kern", .value = 1 } });
  text->setBounds(Rect::MakeWH(240.0f, 80.0f));
  window->setRoot(text);
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap snapshot;
  ASSERT_TRUE(window->snapshot(&snapshot));
  const graphics::Pixmap pixmap = snapshot.makePixmap();
  size_t visible = 0;
  for (int y = 0; y < pixmap.height(); ++y) {
    for (int x = 0; x < pixmap.width(); ++x) {
      const uint32_t argb = pixmap.getPixelARGB32(x, y);
      if ((argb & 0xFF000000u) != 0 && (argb & 0x00FFFFFFu) != 0) ++visible;
    }
  }
  EXPECT_GT(visible, 0u) << "text disappeared over a fully transparent clear";
}

TEST(UiContent, ContentViewsInFlexReceiveRowBounds) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 64;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto host = std::make_shared<ContentHost>();
  auto row = std::make_shared<Flex>(Axis::Row);
  auto left = std::make_shared<ContentView>(host);
  auto right = std::make_shared<ContentView>(host);
  LayoutParams grow;
  grow.grow = 1.0f;
  left->setLayoutParams(grow);
  right->setLayoutParams(grow);
  row->addChild(left);
  row->addChild(right);
  window->setRoot(row);
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();

  EXPECT_FLOAT_EQ(left->bounds().getWidth(), 80.0f);
  EXPECT_FLOAT_EQ(right->bounds().getLeft(), 80.0f);
  EXPECT_FLOAT_EQ(right->bounds().getWidth(), 80.0f);
}

namespace {

class CountingLayer final : public Layer {
 public:
  void draw(RenderContext&) override { ++draws; }
  int draws = 0;
};

} // namespace

TEST(UiContent, FlexRowCallsChildDraw) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 64;
  options.frameLimit = 1;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto row = std::make_shared<Flex>(Axis::Row);
  auto left = std::make_shared<CountingLayer>();
  auto right = std::make_shared<CountingLayer>();
  LayoutParams grow;
  grow.grow = 1.0f;
  left->setLayoutParams(grow);
  right->setLayoutParams(grow);
  row->addChild(left);
  row->addChild(right);
  window->setRoot(row);
  ASSERT_EQ(window->run(), 1);

  EXPECT_EQ(left->draws, 1) << "the Flex row must call draw on its children";
  EXPECT_EQ(right->draws, 1);
  EXPECT_FLOAT_EQ(left->bounds().getWidth(), 80.0f);
}

TEST(UiContent, FlexRowWithStretchPaintsGrowChildren) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 64;
  options.frameLimit = 1;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 0, 255));
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setCrossAlign(Align::Stretch);
  row->setBounds(Rect::MakeWH(160.0f, 64.0f));
  auto left = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  auto right = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  LayoutParams grow;
  grow.grow = 1.0f;
  left->setLayoutParams(grow);
  right->setLayoutParams(grow);
  row->addChild(left);
  row->addChild(right);
  root->addChild(row);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixmap = bitmap.makePixmap();
  ASSERT_TRUE(pixmap.isValid());
  EXPECT_EQ((pixmap.getPixelARGB32(40, 32) >> 16) & 0xFFu, 255u);
  EXPECT_EQ((pixmap.getPixelARGB32(120, 32) >> 8) & 0xFFu, 255u);
}

TEST(UiContent, FlexRootPaintsColorChildWithExplicitBounds) {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 64;
  options.frameLimit = 1;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  auto row = std::make_shared<Flex>(Axis::Row);
  auto left = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  left->setBounds(Rect::MakeWH(80.0f, 64.0f));
  row->addChild(left);
  window->setRoot(row);
  ASSERT_EQ(window->run(), 1);
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixmap = bitmap.makePixmap();
  ASSERT_TRUE(pixmap.isValid());
  EXPECT_EQ((pixmap.getPixelARGB32(40, 32) >> 16) & 0xFFu, 255u);
}
