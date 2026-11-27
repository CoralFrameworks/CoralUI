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
#include <filesystem>
#include <ui/CrashReporter.h>
#include <ui/Flex.h>
#include <ui/FrameStatsOverlay.h>
#include <ui/HotReload.h>
#include <ui/ProcessStats.h>
#include <ui/Motion.h>
#include <ui/NativeAlert.h>
#include <ui/ThemeFile.h>
#include <ui/ThemeHotReload.h>
#include <ui/Element.h>
#include <ui/NumberFormat.h>
#include <ui/OverlayHost.h>
#include <ui/DSL.h>
#include <ui/DSLOverlay.h>
#include <ui/Toast.h>
#include <ui/SplitButton.h>
#include <ui/Menu.h>
#include <ui/Reconciler.h>
#include <ui/DateFormat.h>
#include <ui/Translation.h>
#include <ui/RenderContext.h>
#include <ui/ScrollView.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/Application.h>
#include <ui/Window.h>
#include <ui/WindowChrome.h>
#include <ui/DragDrop.h>
#include <ui/WindowManager.h>
#include <ui/WindowBackend.h>
#include <ui/CanvasBackend.h>
#include <ui/host/ThreadedRenderHost.h>

#include <graphics/Bitmap.h>
#include <graphics/Canvas.h>
#include <graphics/Path.h>
#include <graphics/Typeface.h>

#include <gtest/gtest.h>

#include "TestSupport.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace {

using namespace coral::ui;
namespace dsx = coral::ui::dsl::experimental;

#if defined(_WIN32)
// Catches a regression where the Win32 TaskDialog backend stops being linked and the -1 fallback
// silently takes over again.
TEST(UiNativeAlert, TheWin32BackendAdvertisesNativeAlerts) {
  EXPECT_TRUE(supportsNativeAlerts());
}
#endif

TEST(UiWindow, AHeadlessWindowRendersItsRootAndSnapshots) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);

  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  window->setRoot(root);
  EXPECT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  ASSERT_EQ(bitmap.info().width(), 64);
  ASSERT_EQ(bitmap.info().height(), 64);
  const graphics::Pixmap pixmap = bitmap.makePixmap();
  ASSERT_TRUE(pixmap.isValid());
  const uint8_t* pixel = static_cast<const uint8_t*>(pixmap.pixels());
  // The root covers the whole window, so the top-left texel is the root's red.
  EXPECT_EQ(pixel[0], 255);
  EXPECT_EQ(pixel[1], 0);
  EXPECT_EQ(pixel[2], 0);
  EXPECT_EQ(pixel[3], 255);
}

TEST(UiWindow, ResizeRelayoutsAStretchedRoot) {
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Flex>(Axis::Column);
  root->setCrossAlign(Align::Stretch);
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeWH(20.0f, 20.0f));
  root->addChild(child);
  window->setRoot(root);

  ASSERT_TRUE(window->resize(200, 150));
  EXPECT_FLOAT_EQ(root->bounds().getWidth(), 200.0f);
  EXPECT_FLOAT_EQ(root->bounds().getHeight(), 150.0f);
  EXPECT_FLOAT_EQ(child->bounds().getWidth(), 200.0f);
}

TEST(UiWindow, OpacityAndTranslationCompositeIntoTheFrame) {
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 0, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(20.0f, 20.0f, 60.0f, 60.0f));
  child->setOpacity(0.5f);
  child->setTranslation({ 20.0f, 0.0f });
  root->addChild(child);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  const uint32_t pixel = pixels.getPixelARGB32(60, 40);
  EXPECT_NEAR((pixel >> 16) & 0xFFu, 128u, 3u);
  EXPECT_EQ((pixel >> 8) & 0xFFu, 0u);
}

TEST(UiWindow, RoundedClipCutsTheCornerFromTheFrame) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 0, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 60.0f, 60.0f));
  child->setClip(true);
  child->setClipRadius(16.0f);
  root->addChild(child);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  EXPECT_GT((pixels.getPixelARGB32(35, 35) >> 16) & 0xFFu, 200u);
  EXPECT_LT((pixels.getPixelARGB32(11, 11) >> 16) & 0xFFu, 40u);
}

TEST(UiWindow, ApplicationClearColorFillsResizedArea) {
  auto application = Application::create();
  ASSERT_NE(application, nullptr);
  const auto clear = graphics::Color::MakeRGBA8(10, 20, 30, 255);
  application->setClearColor(clear);
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  auto window = application->createWindow(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Flex>(Axis::Column);
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeWH(20.0f, 20.0f));
  root->addChild(child);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap before;
  ASSERT_TRUE(window->snapshot(&before));
  const auto beforePixel = before.makePixmap().getPixelARGB32(80, 80);
  EXPECT_EQ((beforePixel >> 16) & 0xFFu, 10u);
  EXPECT_EQ((beforePixel >> 8) & 0xFFu, 20u);
  EXPECT_EQ(beforePixel & 0xFFu, 30u);

  ASSERT_TRUE(window->resize(200, 150));
  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap after;
  ASSERT_TRUE(window->snapshot(&after));
  const auto afterPixel = after.makePixmap().getPixelARGB32(180, 130);
  EXPECT_EQ((afterPixel >> 16) & 0xFFu, 10u);
  EXPECT_EQ((afterPixel >> 8) & 0xFFu, 20u);
  EXPECT_EQ(afterPixel & 0xFFu, 30u);
}

TEST(UiWindow, ShrinkingAndGrowingRestoresTextAndScrollHeights) {
  const std::string path = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(path);
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  Window::Options options;
  options.headless = true;
  options.width = 300;
  options.height = 400;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<Flex>(Axis::Column);
  auto input = std::make_shared<TextInput>("text", font);
  input->setPreferredWidth(200.0f);
  auto content = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(30, 30, 30, 255));
  content->setBounds(Rect::MakeWH(200.0f, 160.0f));
  auto scroll = std::make_shared<ScrollView>();
  scroll->setContent(content);
  root->addChild(input);
  root->addChild(scroll);
  window->setRoot(root);

  const float inputHeight = input->bounds().getHeight();
  const float scrollHeight = scroll->bounds().getHeight();
  ASSERT_GT(inputHeight, 0.0f);
  ASSERT_GT(scrollHeight, 0.0f);

  ASSERT_TRUE(window->resize(300, 40));
  ASSERT_TRUE(window->resize(300, 400));
  EXPECT_NEAR(input->bounds().getHeight(), inputHeight, 1.0f);
  EXPECT_NEAR(scroll->bounds().getHeight(), scrollHeight, 1.0f);
}

TEST(UiWindow, AHeadlessWindowLaysOutANestedTree) {
  Window::Options options;
  options.headless = true;
  options.width = 80;
  options.height = 40;
  options.frameLimit = 1;

  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto row = std::make_shared<Flex>(Axis::Row);
  row->setPadding({ 4.0f, 4.0f, 4.0f, 4.0f });
  row->setGap(2.0f);
  auto first = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  first->setBounds(Rect::MakeWH(10.0f, 20.0f));
  auto second = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  second->setBounds(Rect::MakeWH(10.0f, 20.0f));
  row->addChild(first);
  row->addChild(second);

  window->setRoot(row);
  EXPECT_EQ(window->run(), 1);
  EXPECT_FLOAT_EQ(first->bounds().getLeft(), 4.0f);
  EXPECT_FLOAT_EQ(second->bounds().getLeft(), 16.0f);
}

TEST(UiWindow, AnOnDemandWindowDoesNotRenderWithoutARequest) {
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 16;
  options.framePolicy = FramePolicy::OnDemand;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255)));

  // A posted task marks a frame needed, so quit from a task that runs before the first frame is
  // produced and assert that no frame was drawn.
  bool quitBeforeFrame = false;
  window->post([&] {
    window->quit();
    quitBeforeFrame = window->frameCount() == 0;
  });
  EXPECT_EQ(window->run(), 0);
  EXPECT_TRUE(quitBeforeFrame);
}

TEST(UiWindow, AFreshWindowDefaultsToOnDemand) {
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 16;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  EXPECT_EQ(window->framePolicy(), FramePolicy::OnDemand);
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255)));
  EXPECT_EQ(window->run(), 1);
}

TEST(UiWindow, ARequestFrameWakesAnOnDemandWindow) {
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 16;
  options.framePolicy = FramePolicy::OnDemand;
  options.frameLimit = 2;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255)));

  // Each pump asks for one frame and schedules the next request, so the loop renders exactly as many
  // frames as the limit allows while every one of them came from `requestFrame`.
  auto tick = std::make_shared<std::function<void()>>();
  *tick = [&] {
    window->requestFrame();
    window->post(*tick);
  };
  window->post(*tick);

  EXPECT_EQ(window->run(), 2);
  EXPECT_EQ(window->framePolicy(), FramePolicy::OnDemand);
}

TEST(UiWindow, AFrameLimitProducesFramesWithoutExternalRequests) {
  Window::Options options;
  options.headless = true;
  options.width = 8;
  options.height = 8;
  options.framePolicy = FramePolicy::OnDemand;
  options.frameLimit = 5;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(10, 20, 30, 255)));
  EXPECT_EQ(window->run(), 5);
}

TEST(UiWindow, FrameTimingsAreRecorded) {
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 128, 0, 255)));
  ASSERT_EQ(window->run(), 1);

  const FrameTimings timings = window->lastFrameTimings();
  EXPECT_TRUE(timings.valid);
  EXPECT_GE(timings.total, timings.draw + timings.update + timings.present);
  EXPECT_GT(timings.total, 0.0);
}

TEST(UiWindow, AHeadlessWindowReportsItsRendererCapabilities) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  const RendererCapabilities& capabilities = window->rendererCapabilities();
  EXPECT_TRUE(capabilities.recording);
  EXPECT_FALSE(capabilities.backend.empty());
  EXPECT_FALSE(capabilities.device.empty());
  EXPECT_GT(capabilities.maxTextureDimension, 0u);
  EXPECT_GE(capabilities.sampleCount, 1u);
  EXPECT_TRUE(capabilities.externalImages.supported);
  EXPECT_TRUE(capabilities.layerMasks.supported);
  EXPECT_TRUE(capabilities.groupOpacity.supported);
  EXPECT_TRUE(capabilities.blendModes.supported);
  EXPECT_TRUE(capabilities.readback.supported);
  EXPECT_TRUE(capabilities.shapedClips.supported)
    << "the GPU realises a shaped clip through a mask-composited layer";
  // Partial updates are a hint: platforms without a preserved target fall back to a full repaint.
  EXPECT_TRUE(capabilities.damageRects.supported);
}

TEST(UiWindow, ARasterContextReportsSoftwareCapabilities) {
  auto bitmap = graphics::Bitmap::allocate(8, 8);
  ASSERT_FALSE(bitmap.isEmpty());
  auto canvas = graphics::Canvas::create(bitmap);
  if (canvas == nullptr) {
    GTEST_SKIP() << "no raster canvas could be created";
  }

  Canvas uiCanvas = detail::makeCanvas(*canvas);
  RenderContext context(uiCanvas);
  const RendererCapabilities& capabilities = context.capabilities();
  EXPECT_FALSE(capabilities.recording);
  EXPECT_EQ(capabilities.backend, "software");
  EXPECT_FALSE(capabilities.externalImages.supported);
  EXPECT_TRUE(capabilities.shapedClips.supported) << "the CPU rasterises a path clip";
  EXPECT_FALSE(context.supportsExternalImage());
}

TEST(UiWindow, AChildBlendModeCompositesTheGroup) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(16.0f, 16.0f, 32.0f, 32.0f));
  child->setBlendMode(graphics::BlendMode::kMultiply);
  root->addChild(child);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  // Red * blue is black where they overlap; the root stays blue outside it.
  const uint32_t overlap = pixels.getPixelARGB32(24, 24);
  EXPECT_LE((overlap >> 16) & 0xFFu, 8u) << "multiply must combine red with the blue underneath";
  EXPECT_LE((overlap >> 8) & 0xFFu, 8u);
  EXPECT_LE(overlap & 0xFFu, 8u);
  const uint32_t outside = pixels.getPixelARGB32(4, 4);
  EXPECT_EQ((outside >> 16) & 0xFFu, 0u);
  EXPECT_EQ(outside & 0xFFu, 255u);
}

TEST(UiLayout, TransformOriginMovesTheHitArea) {
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 0, 255));
  root->setBounds(Rect::MakeWH(200.0f, 200.0f));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(10.0f, 10.0f, 40.0f, 40.0f));
  child->setScale(2.0f);
  root->addChild(child);

  // Scaling around the centre moves the visible square to (-5, -5)..(55, 55).
  EXPECT_NE(child->hitTest(Point{ 12.0f, 12.0f }), nullptr);
  EXPECT_EQ(child->hitTest(Point{ 68.0f, 68.0f }), nullptr);

  child->setTransformOrigin({ 0.0f, 0.0f });
  EXPECT_NE(child->hitTest(Point{ 68.0f, 68.0f }), nullptr)
    << "a top-left origin scales the layer away from its own corner";
  EXPECT_EQ(child->hitTest(Point{ 4.0f, 4.0f }), nullptr);
}

TEST(UiWindow, APathClipMasksTheLayerAndItsHits) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeWH(64.0f, 64.0f));
  child->setClip(true);
  graphics::Path clip;
  clip.moveTo({ 16.0f, 16.0f });
  clip.lineTo({ 48.0f, 16.0f });
  clip.lineTo({ 48.0f, 48.0f });
  clip.lineTo({ 16.0f, 48.0f });
  clip.close();
  child->setClipPath(clip);
  root->addChild(child);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  EXPECT_EQ((pixels.getPixelARGB32(32, 32) >> 16) & 0xFFu, 255u)
    << "inside the path the red child shows";
  EXPECT_EQ(pixels.getPixelARGB32(4, 4) & 0x00FFFFFFu, 255u) << "outside it the blue root shows";
  EXPECT_EQ(child->hitTest({ 4.0f, 4.0f }), nullptr) << "the clip also bounds hit testing";
  EXPECT_EQ(child->hitTest({ 32.0f, 32.0f }), child.get());

  child->clearClipPath();
  EXPECT_TRUE(child->clipPath().isEmpty());
  EXPECT_NE(child->hitTest({ 4.0f, 4.0f }), nullptr);
}

TEST(UiWindow, ACanvasClipPathIsHonouredOnTheGpu) {
  // `Layer::setClipPath` uses the layer-mask route; this exercises the *Canvas* clip path, which is a
  // per-command mask. The GPU renderer rewrites a masked run as a masked layer, so the pixel test is
  // the evidence that the rewrite (and its layer parenting) is correct.
  class ClippedPainter final : public Layer {
   public:
    void drawSelf(RenderContext& context) override {
      auto& canvas = context.canvas();
      canvas.save();
      graphics::Path clip;
      clip.moveTo({ 16.0f, 16.0f });
      clip.lineTo({ 48.0f, 16.0f });
      clip.lineTo({ 16.0f, 48.0f });
      clip.close();
      canvas.clipPath(clip);
      graphics::Paint red;
      red.color = graphics::Color::MakeRGBA8(255, 0, 0, 255);
      red.style = graphics::Paint::Style::kFill;
      canvas.drawRect(bounds(), red);
      canvas.restore();

      // Content after the clip's restore proves the synthetic mask layer closed and its parent pass
      // came back: this is the regression the SVG corpus caught.
      graphics::Paint green;
      green.color = graphics::Color::MakeRGBA8(0, 255, 0, 255);
      green.style = graphics::Paint::Style::kFill;
      canvas.drawRect(Rect::MakeLTRB(0.0f, 0.0f, 8.0f, 8.0f), green);
    }
  };

  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<ClippedPainter>();
  root->setBounds(Rect::MakeWH(64.0f, 64.0f));
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  EXPECT_EQ(pixels.getPixelARGB32(24, 24) & 0x00FFFFFFu, 0x00FF0000u);
  EXPECT_EQ(pixels.getPixelARGB32(40, 40) & 0x00FFFFFFu, 0u)
    << "inside the mask's rectangle but outside the triangle, the shaped clip must reject";
  EXPECT_EQ(pixels.getPixelARGB32(4, 4) & 0x0000FF00u, 0x0000FF00u)
    << "content after the clip's restore still draws";
  EXPECT_EQ(pixels.getPixelARGB32(60, 60) & 0x00FFFFFFu, 0u)
    << "outside the shaped clip the frame stays cleared";
}

TEST(UiWindow, ASizedOpacityGroupKeepsItsDevicePosition) {
  // A top-level, text-free opacity group is exactly what R1 step one sizes. The pixel check is what
  // proves the layer's commands were translated by its origin and the composite sampled 0..1 instead
  // of the old region/root mapping.
  class RedGroup final : public Layer {
   public:
    void drawSelf(RenderContext& context) override {
      auto& canvas = context.canvas();
      graphics::Paint red;
      red.color = graphics::Color::MakeRGBA8(255, 0, 0, 255);
      red.style = graphics::Paint::Style::kFill;
      red.antiAlias = false;
      canvas.drawRect(bounds(), red);

      // A stencil path in the same sized layer: it exercises the point translation, which a rectangle
      // (a single quad) never would.
      graphics::Path triangle;
      triangle.moveTo({ 60.0f, 30.0f });
      triangle.lineTo({ 75.0f, 30.0f });
      triangle.lineTo({ 60.0f, 45.0f });
      triangle.close();
      graphics::Paint green;
      green.color = graphics::Color::MakeRGBA8(0, 255, 0, 255);
      green.style = graphics::Paint::Style::kFill;
      green.antiAlias = false;
      canvas.drawPath(triangle, green);
    }
  };

  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 100;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto group = std::make_shared<RedGroup>();
  group->setBounds(Rect::MakeLTRB(20.0f, 20.0f, 80.0f, 80.0f));
  group->setOpacity(0.5f);
  // `Window::setRoot` stretches its root to the window, so the group has to be a child for its bounds
  // to stay at 20..80 and exercise the sizing path.
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 0, 255));
  root->addChild(group);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  const uint32_t center = pixels.getPixelARGB32(50, 50);
  EXPECT_GT((center >> 16) & 0xFFu, 100u) << "the group's red is at its own device position";
  EXPECT_LT((center >> 16) & 0xFFu, 160u) << "group opacity still applies";
  EXPECT_EQ(pixels.getPixelARGB32(10, 10) & 0x00FFFFFFu, 0u);
  EXPECT_EQ(pixels.getPixelARGB32(15, 50) & 0x00FFFFFFu, 0u)
    << "a sized target must not shift the group's content into its own local origin";
  const uint32_t pathPixel = pixels.getPixelARGB32(65, 35);
  EXPECT_GT((pathPixel >> 8) & 0xFFu, 100u)
    << "a path inside the sized layer keeps its translated points (green blended by group opacity)";
}

TEST(UiWindow, ARoundedClipMaskCoversTheLayerRegionNotTheWholeTarget) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(16.0f, 16.0f, 48.0f, 48.0f));
  child->setClip(true);
  child->setClipRadius(12.0f);
  root->addChild(child);
  window->setRoot(root);
  ASSERT_EQ(window->run(), 1);

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  EXPECT_EQ(pixels.getPixelARGB32(32, 32) & 0x00FFFFFFu, 0x00FF0000u)
    << "the layer's centre is inside the rounded rect";
  EXPECT_EQ(pixels.getPixelARGB32(17, 17) & 0x00FFFFFFu, 0x000000FFu)
    << "the corner is outside the rounded rect; a target-sized mask squashed over the layer puts "
       "the corner in the wrong place and paints red here";
}

TEST(UiWindow, ACleanFrameReplaysTheRetainedRecording) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());
  const coral::ui::FrameTimings first = window->lastFrameTimings();
  EXPECT_GT(first.draw, 0.0) << "the first frame records the tree";

  // Nothing changed: the window must reuse the recording instead of walking the tree again.
  ASSERT_TRUE(window->renderFrame());
  const coral::ui::FrameTimings second = window->lastFrameTimings();
  EXPECT_EQ(second.draw, 0.0) << "a clean subtree should not be recorded again";
  EXPECT_EQ(second.update, 0.0);

  // The retained frame still shows the root.
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  EXPECT_EQ(bitmap.makePixmap().getPixelARGB32(32, 32) & 0x00FFFFFFu, 0x00FF0000u);

  // A model change invalidates the retained recording and the next frame records again.
  root->setColor(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  ASSERT_TRUE(window->renderFrame());
  EXPECT_GT(window->lastFrameTimings().draw, 0.0);
  ASSERT_TRUE(window->snapshot(&bitmap));
  EXPECT_EQ(bitmap.makePixmap().getPixelARGB32(32, 32) & 0x00FFFFFFu, 0x0000FF00u);
}

TEST(UiWindow, ADamageRectUpdatesOnlyTheDirtyPixels) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 16.0f, 16.0f));
  root->addChild(child);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());

  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  ASSERT_EQ(bitmap.makePixmap().getPixelARGB32(8, 8) & 0x00FFFFFFu, 0x00FF0000u);

  child->setColor(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  window->invalidate(Rect::MakeLTRB(0.0f, 0.0f, 16.0f, 16.0f));
  EXPECT_EQ(window->damage().getWidth(), 16.0f);
  ASSERT_TRUE(window->renderFrame());
  EXPECT_TRUE(window->damage().isEmpty()) << "a submitted damage rect is consumed";

  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  EXPECT_EQ(pixels.getPixelARGB32(8, 8) & 0x00FFFFFFu, 0x0000FF00u);
  EXPECT_EQ(pixels.getPixelARGB32(40, 40) & 0x00FFFFFFu, 0x000000FFu)
    << "pixels outside the damage come from the previous frame";
}

TEST(UiWindow, ALeafRecordingIsReusedUntilTheLayerIsDirty) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 1024;
  options.height = 768;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  // Larger than the whole-subtree cache limit, so the root is recorded normally and the child leaf
  // owns the cache this test inspects.
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(8.0f, 8.0f, 24.0f, 24.0f));
  root->addChild(child);
  window->setRoot(root);

  ASSERT_TRUE(window->renderFrame());
  EXPECT_TRUE(child->hasCachedRecording());
  root->setNeedsPaint();
  ASSERT_TRUE(window->renderFrame());
  EXPECT_TRUE(child->hasCachedRecording());
  child->setColor(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  EXPECT_FALSE(child->hasCachedRecording()) << "changing the paint invalidates the list";

  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  EXPECT_GT(bitmap.makePixmap().getPixelARGB32(12, 12) & 0x00FF00u, 0x00F000u);
}

#if defined(_WIN32) || defined(__linux__)
TEST(UiWindow, ANativeWindowReplaysARetainedRecording) {
  Window::Options options;
  options.headless = false;
  options.width = 320;
  options.height = 200;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no native window could be created in this session";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  EXPECT_EQ(window->lastFrameTimings().draw, 0.0)
    << "a clean native window replays the recording instead of recording it";
}
TEST(UiWindow, ANativeWindowAppliesADamageRect) {
  Window::Options options;
  options.headless = false;
  options.width = 320;
  options.height = 200;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no native window could be created in this session";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 40.0f, 40.0f));
  root->addChild(child);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  child->setColor(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  window->invalidate(Rect::MakeLTRB(0.0f, 0.0f, 40.0f, 40.0f));
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  EXPECT_EQ(pixels.getPixelARGB32(10, 10) & 0x00FFFFFFu, 0x0000FF00u);
  EXPECT_EQ(pixels.getPixelARGB32(200, 120) & 0x00FFFFFFu, 0x000000FFu);
}

TEST(UiWindow, ANativeWindowSurvivesResize) {
  Window::Options options;
  options.headless = false;
  options.width = 120;
  options.height = 90;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
#if defined(__linux__)
  options.platform = Window::NativePlatform::X11; // exercise XIM/XIC creation on WSLg
#endif
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no native window could be created in this session";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 128, 0, 255)));
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  ASSERT_TRUE(window->resize(160, 120)) << window->diagnostics();
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap)) << window->diagnostics();
  EXPECT_EQ(bitmap.width(), window->width());
  EXPECT_EQ(bitmap.height(), window->height());
  if (window->width() == 120) {
    GTEST_SKIP() << "the window manager refused the resize; surface still matches the client";
  }
  EXPECT_GT(window->width(), 120) << "WM/DPI may clamp the requested device size";
}

TEST(UiWindow, ANativeWindowHonoursAnExplicitUiScale) {
  Window::Options options;
  options.headless = false;
  options.width = 96;
  options.height = 96;
  options.frameLimit = 1;
  options.uiScale = 1.5f;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no native window could be created in this session";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 20.0f, 20.0f));
  root->addChild(child);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  EXPECT_EQ(pixels.getPixelARGB32(24, 24) & 0x00FFFFFFu, 0x00FF0000u)
    << "logical 20px * 1.5 lands at device 30";
  EXPECT_EQ(pixels.getPixelARGB32(45, 45) & 0x00FFFFFFu, 0x000000FFu);
}

TEST(UiWindow, AnApplicationSharesTheDeviceBetweenNativeWindows) {
  auto application = Application::create();
  Window::Options options;
  options.headless = false;
  options.width = 96;
  options.height = 96;
  options.frameLimit = 1;
  auto first = application->createWindow(options);
  auto second = application->createWindow(options);
  if (first == nullptr || second == nullptr) {
    GTEST_SKIP() << "no native window could be created in this session";
  }
  EXPECT_EQ(detail::deviceOf(*first), detail::deviceOf(*second));
  first->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255)));
  second->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255)));
  ASSERT_TRUE(first->renderFrame()) << first->diagnostics();
  ASSERT_TRUE(second->renderFrame()) << second->diagnostics();
}

TEST(UiWindow, ANativeOnDemandWakesFromAPostedTask) {
  Window::Options options;
  options.headless = false;
  options.width = 96;
  options.height = 96;
  options.framePolicy = FramePolicy::OnDemand;
  options.frameLimit = 2;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no native window could be created in this session";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 128, 0, 255)));
  auto tick = std::make_shared<std::function<void()>>();
  *tick = [&] {
    window->requestFrame();
    window->post(*tick);
  };
  window->post(*tick);
  EXPECT_EQ(window->run(), 2);
}

#if defined(_WIN32)
TEST(UiWindow, AWin32DpiChangeUpdatesTheUiScale) {
  Window::Options options;
  options.headless = false;
  options.width = 160;
  options.height = 120;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no native window could be created in this session";
  }
  auto hwnd = static_cast<HWND>(window->nativeHandle());
  ASSERT_NE(hwnd, nullptr);
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 128, 0, 255)));
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();

  RECT outer{};
  GetWindowRect(hwnd, &outer);
  SendMessageW(hwnd, WM_DPICHANGED, MAKEWPARAM(144, 144), reinterpret_cast<LPARAM>(&outer));
  EXPECT_NEAR(window->dpiScale(), 1.5f, 0.01f);
  EXPECT_NEAR(window->uiScale(), 1.5f, 0.01f) << "automatic uiScale follows the DPI change";
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
}

TEST(UiWindow, AMoveToAnotherMonitorKeepsRendering) {
  struct Monitors {
    std::vector<RECT> rects;
  } monitors;
  EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR, HDC, LPRECT rect, LPARAM data) -> BOOL {
    reinterpret_cast<Monitors*>(data)->rects.push_back(*rect);
    return TRUE;
  }, reinterpret_cast<LPARAM>(&monitors));
  if (monitors.rects.size() < 2) {
    GTEST_SKIP() << "this session has a single monitor";
  }

  Window::Options options;
  options.headless = false;
  options.width = 160;
  options.height = 120;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no native window could be created in this session";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 128, 0, 255)));
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  auto hwnd = static_cast<HWND>(window->nativeHandle());
  ASSERT_NE(hwnd, nullptr);

  const RECT& target = monitors.rects[1];
  SetWindowPos(hwnd, nullptr, target.left + 8, target.top + 8,
    target.right - target.left - 16, target.bottom - target.top - 16,
    SWP_NOZORDER | SWP_NOACTIVATE);
  window->pump();
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  EXPECT_FALSE(bitmap.isEmpty());
}
#endif

#endif

TEST(UiWindow, AControlChangeProducesItsOwnDamageRect) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 16.0f, 16.0f));
  root->addChild(child);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());

  // No explicit invalidate: setNeedsPaint on the control is the only damage source.
  child->setColor(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  EXPECT_EQ(pixels.getPixelARGB32(8, 8) & 0x00FFFFFFu, 0x0000FF00u);
  EXPECT_EQ(pixels.getPixelARGB32(40, 40) & 0x00FFFFFFu, 0x000000FFu);
}

TEST(UiWindow, ATranslationDamagesBothOldAndNewPositions) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto child = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  child->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 16.0f, 16.0f));
  root->addChild(child);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());

  child->setTranslation({ 32.0f, 0.0f });
  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  EXPECT_EQ(pixels.getPixelARGB32(8, 8) & 0x00FFFFFFu, 0x000000FFu)
    << "the old position is restored from the parent";
  EXPECT_EQ(pixels.getPixelARGB32(40, 8) & 0x00FFFFFFu, 0x00FF0000u);
}

TEST(UiWindow, AScrolledViewportClipsItsContentDamage) {
  Window::Options options;
  options.headless = true;
  options.width = 128;
  options.height = 128;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255));
  auto scroll = std::make_shared<ScrollView>();
  scroll->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 64.0f, 64.0f));
  auto content = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  content->setBounds(Rect::MakeLTRB(0.0f, 0.0f, 64.0f, 256.0f));
  scroll->setContent(content);
  root->addChild(scroll);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());

  scroll->setScrollOffset(0.0f, 16.0f);
  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const graphics::Pixmap pixels = bitmap.makePixmap();
  EXPECT_EQ(pixels.getPixelARGB32(8, 8) & 0x00FFFFFFu, 0x0000FF00u);
  EXPECT_EQ(pixels.getPixelARGB32(100, 100) & 0x00FFFFFFu, 0x000000FFu)
    << "content damage is clipped to the viewport";
}

TEST(UiWindow, AnApplicationDrivesTwoWindows) {
  auto application = Application::create();
  ASSERT_NE(application, nullptr);
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 16;
  options.frameLimit = 1;
  auto first = application->createWindow(options);
  auto second = application->createWindow(options);
  if (first == nullptr || second == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  first->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255)));
  second->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255)));
  EXPECT_EQ(application->run(), 2);
  EXPECT_EQ(first->frameCount(), 1);
  EXPECT_EQ(second->frameCount(), 1);
  EXPECT_EQ(detail::deviceOf(*first), detail::deviceOf(*second)) << "headless surfaces share the application device";
}

TEST(UiWindow, AnApplicationAppliesItsThemeAndReportsCloses) {
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  options.frameLimit = 1;
  auto owner = application->createWindow(options);
  auto borrower = application->createWindow(options);
  if (owner == nullptr || borrower == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  application->setTheme(Theme::light());
  EXPECT_EQ(owner->theme().background, Theme::light().background);
  EXPECT_EQ(borrower->theme().background, Theme::light().background);
  auto later = application->createWindow(options);
  if (later != nullptr) EXPECT_EQ(later->theme().background, Theme::light().background);

  int closed = 0;
  application->setOnWindowClosed([&](Window*) { ++closed; });
  owner->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255)));
  application->run();
  borrower->quit();
  application->run();
  EXPECT_EQ(closed, later != nullptr ? 3 : 2);
}

TEST(UiWindow, AnApplicationKeepsTheDeviceAfterAllWindowsClose) {
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  options.frameLimit = 1;
  auto owner = application->createWindow(options);
  auto borrower = application->createWindow(options);
  if (owner == nullptr || borrower == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  owner->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255)));
  borrower->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 255, 255)));
  borrower->quit();
  EXPECT_EQ(application->run(), 1);
  EXPECT_TRUE(application->windows().empty()) << "closed headless windows are removed";
  // The device is owned by Application, not by the now-closed first window: a later window can still
  // borrow it.
  auto later = application->createWindow(options);
  if (later != nullptr) EXPECT_EQ(detail::deviceOf(*later), detail::deviceOf(*owner));
}

TEST(UiWindow, AnApplicationRunsAPostedTaskOnTheUiThread) {
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  auto window = application->createWindow(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  bool ran = false;
  std::thread::id taskThread;
  ASSERT_TRUE(application->post([&] {
    ran = true;
    taskThread = std::this_thread::get_id();
    application->quit();
  }));
  application->run();
  EXPECT_TRUE(ran);
  EXPECT_EQ(taskThread, std::this_thread::get_id())
    << "posted tasks run on the thread that drives the application";
}

TEST(UiWindow, AnApplicationFiresADelayedTaskWhileNoWindowProducesFrames) {
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  // frameLimit 0 means the windows never render on their own: only the timer can wake the loop.
  options.frameLimit = 0;
  auto first = application->createWindow(options);
  auto second = application->createWindow(options);
  if (first == nullptr || second == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  bool fired = false;
  const auto start = std::chrono::steady_clock::now();
  ASSERT_TRUE(application->postDelayed([&] {
    fired = true;
    application->quit();
  }, std::chrono::milliseconds(30)));
  application->run();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - start);

  EXPECT_TRUE(fired) << "a delayed task must run even when no window requests a frame";
  EXPECT_GE(elapsed.count(), 20) << "the task must not be run before its delay";
}

TEST(UiWindow, AnApplicationTimerRepeatsOnTheUiThread) {
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 32;
  auto window = application->createWindow(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  int fires = 0;
  auto timer = application->createTimer(std::chrono::milliseconds(10), [&] {
    ++fires;
    if (fires >= 3) application->quit();
  }, true);
  ASSERT_NE(timer, nullptr);
  timer->start();
  application->run();
  EXPECT_GE(fires, 3);
  timer->stop();
  EXPECT_FALSE(timer->running());
}

TEST(UiWindow, ImeCandidatesNavigateAndClose) {
  Window::Options options;
  options.headless = true;
  options.width = 240;
  options.height = 120;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  window->setRoot(std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(24, 26, 32, 255)));
  window->setImeCandidates({ "1. one", "2. two", "3. three" }, 1);
  EXPECT_TRUE(window->imeCandidatesVisible());
  EXPECT_EQ(window->imeCandidateCount(), 3u);
  EXPECT_EQ(window->imeCandidateSelected(), 1u);

  KeyEvent down;
  down.key = Key::Down;
  window->keyDown(down);
  EXPECT_EQ(window->imeCandidateSelected(), 2u);
  KeyEvent escape;
  escape.key = Key::Escape;
  window->keyDown(escape);
  EXPECT_FALSE(window->imeCandidatesVisible());

  window->setImeCandidates({ "alpha", "beta" });
  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  EXPECT_FALSE(bitmap.isEmpty());
}

TEST(UiWindow, SmallWhiteTextOnBlackUsesSubpixelCoverage) {
  const std::string fontPath = std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
  const auto typeface = graphics::Typeface::fromFile(fontPath);
  if (typeface == nullptr) GTEST_SKIP() << "the UI test font is not available";
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);
  const uint32_t glyph = font->glyphForCodePoint(U'A');
  if (glyph == 0 || !typeface->glyphBitmap(glyph, font->size(), true).subpixel) {
    GTEST_SKIP() << "this platform has no subpixel glyph rasteriser";
  }

  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 48;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(0, 0, 0, 255);
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<ColorLayer>(graphics::Color::MakeRGBA8(0, 0, 0, 255));
  auto text = std::make_shared<TextLayer>("AVA", font,
    graphics::Color::MakeRGBA8(255, 255, 255, 255));
  text->setBounds(Rect::MakeWH(160.0f, 48.0f));
  root->addChild(text);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());

  graphics::Bitmap frame;
  ASSERT_TRUE(window->snapshot(&frame));
  const graphics::Pixmap pixels = frame.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  int greatestChannelDelta = 0;
  for (int y = 0; y < frame.height(); ++y) {
    for (int x = 0; x < frame.width(); ++x) {
      const uint32_t argb = pixels.getPixelARGB32(x, y);
      const int red = static_cast<int>((argb >> 16) & 0xFFu);
      const int green = static_cast<int>((argb >> 8) & 0xFFu);
      const int blue = static_cast<int>(argb & 0xFFu);
      int high = red;
      if (green > high) high = green;
      if (blue > high) high = blue;
      int low = red;
      if (green < low) low = green;
      if (blue < low) low = blue;
      if (high - low > greatestChannelDelta) greatestChannelDelta = high - low;
    }
  }
  EXPECT_GT(greatestChannelDelta, 8)
    << "ClearType-style coverage should leave a visible per-channel difference at glyph edges";
}

TEST(UiWindow, AWindowDrivesFramesThroughTheRendererContract) {
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 16;
  options.frameLimit = 1;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no device is available for the headless surface";

  Renderer& renderer = *window;
  EXPECT_NE(renderer.rendererKind(), RendererKind::Unknown);
  EXPECT_FALSE(renderer.capabilities().backend.empty());
  EXPECT_TRUE(renderer.renderFrame());

  graphics::Bitmap bitmap;
  EXPECT_TRUE(renderer.snapshot(&bitmap));
  EXPECT_FALSE(bitmap.isEmpty());
  EXPECT_TRUE(renderer.recover()) << "a healthy headless renderer can recreate its surface";
}

TEST(UiWindow, ARasterWindowRendersWithoutADevice) {
  Window::Options options;
  options.width = 8;
  options.height = 6;
  options.rendererBackend = RendererBackend::Raster;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  EXPECT_EQ(window->rendererKind(), RendererKind::Raster);
  EXPECT_EQ(window->rendererDevice(), nullptr);
  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 140, 60, 255)));

  ASSERT_TRUE(window->renderFrame());
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));
  const auto pixels = bitmap.makePixmap();
  const uint32_t pixel = pixels.getPixelARGB32(4, 3);
  EXPECT_EQ((pixel >> 16) & 0xFFu, 20u);
  EXPECT_EQ((pixel >> 8) & 0xFFu, 140u);
  EXPECT_EQ(pixel & 0xFFu, 60u);
}

TEST(UiWindow, AThreadedHeadlessWindowRendersThroughItsRenderHost) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 12;
  options.renderHost = RenderHostKind::RenderThread;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr) << "the threaded render host could not start";
  EXPECT_EQ(window->rendererKind(), RendererKind::GPU);
  EXPECT_FALSE(window->isRenderTargetOnScreen()) << "a headless target is not a window";

  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(200, 40, 70, 255)));
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap)) << window->diagnostics();
  const auto pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  const uint32_t pixel = pixels.getPixelARGB32(8, 6);
  EXPECT_EQ((pixel >> 16) & 0xFFu, 200u);
  EXPECT_EQ((pixel >> 8) & 0xFFu, 40u);
  EXPECT_EQ(pixel & 0xFFu, 70u);
  EXPECT_TRUE(window->pump()) << "drains the render host's presentation task";
  EXPECT_TRUE(window->lastFrameTimings().valid);

  ASSERT_TRUE(window->recoverDevice()) << window->diagnostics();
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  ASSERT_TRUE(window->snapshot(&bitmap)) << window->diagnostics();
  EXPECT_EQ(window->rendererKind(), RendererKind::GPU);

  ASSERT_TRUE(window->resize(24, 18)) << window->diagnostics();
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  ASSERT_TRUE(window->snapshot(&bitmap)) << window->diagnostics();
  EXPECT_EQ(bitmap.width(), 24);
  EXPECT_EQ(bitmap.height(), 18);
}

TEST(UiWindow, WindowAppearanceStateChangesAtRuntime) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  EXPECT_EQ(window->title(), "Coral UI");
  window->setTitle("Renamed window");
  EXPECT_EQ(window->title(), "Renamed window");

  EXPECT_EQ(window->minimumWidth(), 0);
  window->setMinimumSize(120, 80);
  EXPECT_EQ(window->minimumWidth(), 120);
  EXPECT_EQ(window->minimumHeight(), 80);

  EXPECT_FALSE(window->isFullscreen());
  window->setFullscreen(true);
  EXPECT_TRUE(window->isFullscreen());
  window->setFullscreen(false);
  EXPECT_FALSE(window->isFullscreen());

  const Rect bounds = window->bounds();
  EXPECT_EQ(bounds.getWidth(), 200.0f);
  EXPECT_EQ(bounds.getHeight(), 120.0f);

  window->setPosition(40, 50);
  window->restoreGeometry(Rect::MakeLTRB(10.0f, 20.0f, 330.0f, 260.0f));
  EXPECT_EQ(window->width(), 320);
  EXPECT_EQ(window->height(), 240);
}

TEST(UiWindow, FrameStatisticsAccumulateAcrossFrames) {
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 24;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 20, 30, 255)));
  ASSERT_TRUE(window->renderFrame());
  ASSERT_TRUE(window->renderFrame());
  const FrameStatistics stats = window->frameStatistics();
  EXPECT_EQ(stats.frames, 2u);
  EXPECT_GT(stats.lastTotalMilliseconds, 0.0);
  EXPECT_GT(stats.averageTotalMilliseconds, 0.0);
  EXPECT_EQ(stats.recoveries, 0u);
}

TEST(UiWindow, FrameStatsOverlayFormatsStatistics) {
  FrameStatistics statistics;
  statistics.frames = 42;
  statistics.averageTotalMilliseconds = 1.5;
  statistics.lastTotalMilliseconds = 2.0;
  statistics.recoveries = 3;
  statistics.lastPresentMilliseconds = 0.75;
  statistics.hostKind = RenderHostKind::RenderThread;
  statistics.refreshHertz = 60.0;
  statistics.queueDepth = 1;
  statistics.droppedFrames = 5;
  statistics.failedFrames = 2;
  statistics.layers = 17;
  statistics.visibleLayers = 15;
  statistics.lastRenderMilliseconds = 1.25;
  statistics.lastExecuteMilliseconds = 0.5;
  statistics.lastGpuMilliseconds = 0.75;
  statistics.residentBytes = 24ull * 1024 * 1024;
  statistics.renderStages.record = 0.75;
  statistics.renderStages.fields = 0.25;
  statistics.renderStages.image = 0.5;
  statistics.renderStages.path = 0.125;
  const std::string text = FrameStatsOverlay::format(statistics);
  EXPECT_NE(text.find("frames 42"), std::string::npos) << text;
  EXPECT_NE(text.find("avg 1.50 ms"), std::string::npos) << text;
  EXPECT_NE(text.find("last 2.00 ms"), std::string::npos) << text;
  EXPECT_NE(text.find("present 0.75 ms"), std::string::npos) << text;
  EXPECT_NE(text.find("render-thread"), std::string::npos) << text;
  EXPECT_NE(text.find("60 Hz"), std::string::npos) << text;
  EXPECT_NE(text.find("queue 1"), std::string::npos) << text;
  EXPECT_NE(text.find("record 0.75"), std::string::npos) << text;
  EXPECT_NE(text.find("draws image 0.50"), std::string::npos) << text;
  EXPECT_NE(text.find("drops 5"), std::string::npos) << text;
  EXPECT_NE(text.find("fails 2"), std::string::npos) << text;
  EXPECT_NE(text.find("layers 17 (15 visible)"), std::string::npos) << text;
  EXPECT_NE(text.find("recoveries 3"), std::string::npos) << text;
  EXPECT_NE(text.find("render 1.25 ms"), std::string::npos) << text;
  EXPECT_NE(text.find("submit 0.50 ms"), std::string::npos) << text;
  EXPECT_NE(text.find("gpu 0.75 ms"), std::string::npos) << text;
  EXPECT_NE(text.find("vram 24.0 MiB"), std::string::npos) << text;
}

TEST(UiWindow, FrameStatisticsReportTheHostAndContentSize) {
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 24;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  auto root = std::make_shared<Layer>();
  auto child = std::make_shared<ColorLayer>(Color::MakeRGBA8(10, 20, 30, 255));
  root->addChild(child);
  auto hidden = std::make_shared<ColorLayer>(Color::MakeRGBA8(1, 2, 3, 255));
  hidden->setVisible(false);
  root->addChild(hidden);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());

  const FrameStatistics stats = window->frameStatistics();
  EXPECT_EQ(stats.hostKind, RenderHostKind::SameThread);
  EXPECT_EQ(stats.queueDepth, 0u) << "the same-thread host presents on the UI thread";
  EXPECT_EQ(stats.droppedFrames, 0u);
  EXPECT_EQ(stats.failedFrames, 0u);
  EXPECT_EQ(stats.layers, 3u) << "root plus two children";
  EXPECT_EQ(stats.visibleLayers, 2u) << "the hidden child is counted but not visible";
}

TEST(UiWindow, ProcessStatisticsReportMemoryAndHandles) {
  const ProcessStatistics stats = processStatistics();
#if defined(_WIN32) || defined(__linux__)
  EXPECT_GT(stats.workingSetBytes, 0u) << "the process has a resident set";
  EXPECT_GT(stats.handles, 0u) << "at least stdin/stdout/stderr are open";
#else
  GTEST_SKIP() << "the platform reports no process counters";
#endif
  const std::string line = formatProcessStatistics(stats);
  EXPECT_NE(line.find("mem "), std::string::npos) << line;
  EXPECT_NE(line.find("handles "), std::string::npos) << line;
}

TEST(UiWindow, CrashReporterWritesAContextReportAndAMinidump) {
  // A directory the test process can certainly write: `%TEMP%` is not writable on every CI sandbox,
  // so the report goes next to the working directory and is removed by the guard.
  struct RemoveOnExit {
    std::string path;
    ~RemoveOnExit() {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  };
  const std::string stem = "coral-crash-" + std::to_string(
    std::chrono::steady_clock::now().time_since_epoch().count());
  const std::string textPath =
    (std::filesystem::current_path() / (stem + ".txt")).string();
  const std::string dumpPath =
    (std::filesystem::current_path() / (stem + ".dmp")).string();
  const RemoveOnExit textGuard{ textPath };
  const RemoveOnExit dumpGuard{ dumpPath };

  setCrashContext("window 320x200 backend raster");
  ASSERT_TRUE(writeCrashTextReport(textPath));
  std::ifstream text(textPath);
  ASSERT_TRUE(text.good());
  const std::string report((std::istreambuf_iterator<char>(text)), std::istreambuf_iterator<char>());
  EXPECT_NE(report.find("window 320x200 backend raster"), std::string::npos) << report;
  EXPECT_NE(report.find("handles "), std::string::npos) << report;
  EXPECT_EQ(crashContext(), "window 320x200 backend raster");

#if defined(_WIN32)
  EXPECT_TRUE(writeMiniDump(dumpPath));
  std::ifstream dump(dumpPath, std::ios::binary);
  ASSERT_TRUE(dump.good());
  char magic[4] = {};
  dump.read(magic, 4);
  EXPECT_EQ(std::string(magic, 4), "MDMP") << "a Windows minidump starts with MDMP";

  CrashReporterOptions options;
  options.directory = std::filesystem::current_path().string();
  EXPECT_TRUE(installCrashReporter(options));
  EXPECT_TRUE(crashReporterInstalled());
  uninstallCrashReporter();
  EXPECT_FALSE(crashReporterInstalled());
#else
  EXPECT_FALSE(writeMiniDump(dumpPath)) << "no minidump format on this platform";
  // POSIX installs signal handlers for SIGSEGV/SIGABRT/SIGBUS/SIGILL; there is no minidump writer,
  // but the text-report path and the install/uninstall state are real.
  EXPECT_TRUE(installCrashReporter(CrashReporterOptions{}));
  EXPECT_TRUE(crashReporterInstalled());
  uninstallCrashReporter();
  EXPECT_FALSE(crashReporterInstalled());
#endif

  setCrashContext({});
}

TEST(UiWindow, FrameStatsOverlayRemembersItsToggle) {
  const auto typeface = graphics::Typeface::fromFile(
    std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf");
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);

  struct RemoveOnExit {
    std::string path;
    ~RemoveOnExit() {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  };
  const std::string settings = (std::filesystem::current_path() /
    ("coral-overlay-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
      ".cfg")).string();
  const RemoveOnExit guard{ settings };

  FrameStatsOverlayOptions options;
  options.settingsPath = settings;
  {
    FrameStatsOverlay overlay(font, Color::White(), options);
    EXPECT_TRUE(overlay.enabled()) << "a first run starts with the configured default";
    overlay.setEnabled(false);
    EXPECT_FALSE(overlay.enabled());
  }
  {
    FrameStatsOverlay overlay(font, Color::White(), options);
    EXPECT_FALSE(overlay.enabled()) << "the toggle survives a restart";
    overlay.setEnabled(true);
  }
  {
    FrameStatsOverlay overlay(font, Color::White(), options);
    EXPECT_TRUE(overlay.enabled());
  }

  // A settings file without a valid line must not silently hide the overlay.
  {
    std::ofstream file(settings, std::ios::trunc);
    file << "not a setting\n";
  }
  FrameStatsOverlay overlay(font, Color::White(), options);
  EXPECT_TRUE(overlay.enabled()) << "an unreadable setting keeps the configured default";
}

TEST(UiWindow, DragOutProviderArmsAPlatformDragWithoutBreakingSelection) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  class DragOutProbe final : public Layer {
   public:
    bool onPointerDown(const PointerEvent&) override {
      ++presses;
      return true;
    }
    bool onPointerMove(const PointerEvent&) override {
      ++moves;
      return false;
    }
    int presses = 0;
    int moves = 0;
  };

  auto probe = std::make_shared<DragOutProbe>();
  probe->setBounds(Rect::MakeWH(64.0f, 64.0f));
  int asked = 0;
  bool offerPayload = true;
  probe->setDragOutProvider([&]() -> std::optional<DragPayload> {
    ++asked;
    return offerPayload ? std::optional<DragPayload>(DragPayload::text("dragged"))
                        : std::optional<DragPayload>{};
  });
  window->setRoot(probe);
  ASSERT_TRUE(window->renderFrame());

  PointerEvent press;
  press.button = PointerButton::Primary;
  press.position = { 10.0f, 10.0f };
  ASSERT_TRUE(window->pointerDown(press));
  EXPECT_EQ(asked, 1) << "the provider is asked once, right after the press";
  EXPECT_EQ(probe->presses, 1);

  // Moving past the threshold asks the platform for a drag. Headless has no drag entry point, so the
  // gesture degrades and the layer keeps receiving moves: a widget's selection still works in tests.
  PointerEvent move = press;
  move.button = PointerButton::None;
  move.position = { 40.0f, 10.0f };
  window->pointerMove(move);
  EXPECT_EQ(probe->moves, 1) << "no platform drag here, so the move reaches the layer";
  EXPECT_EQ(asked, 1) << "the provider is not polled on every move";
  EXPECT_FALSE(window->isDragging()) << "a platform drag is not an internal drag";

  PointerEvent up = move;
  up.button = PointerButton::Primary;
  window->pointerUp(up);

  // A provider that declines leaves the press entirely to the layer.
  offerPayload = false;
  const int movesBefore = probe->moves;
  window->pointerDown(press);
  EXPECT_EQ(asked, 2) << "a later press asks again";
  window->pointerMove(move);
  EXPECT_EQ(probe->moves, movesBefore + 1) << "a declined drag-out does not swallow moves";
  window->pointerUp(up);
}

TEST(UiWindow, TextInputOffersTheSelectionForAPlatformDrag) {
  const auto typeface = graphics::Typeface::fromFile(
    std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf");
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);
  TextInput input("hello world", font);
  input.setBounds(Rect::MakeWH(240.0f, 32.0f));
  input.setSelection(0, 5);
  ASSERT_TRUE(input.hasSelection());

  // A press inside the selection captures the marked text for a drag-out.
  PointerEvent inside;
  inside.button = PointerButton::Primary;
  inside.position = { 4.0f, 16.0f };
  ASSERT_TRUE(input.onPointerDown(inside));
  const auto& provider = input.dragOutProvider();
  ASSERT_TRUE(provider) << "a text field offers its selection for drag-out";
  const std::optional<DragPayload> payload = provider();
  ASSERT_TRUE(payload.has_value());
  EXPECT_EQ(payload->type(), "text/plain");
  EXPECT_EQ(payload->data(), "hello");

  // A press outside the selection (or with no selection) offers nothing.
  input.onPointerUp(inside);
  EXPECT_FALSE(provider().has_value()) << "the payload is cleared on release";
  input.setCursor(0);
  PointerEvent outside = inside;
  outside.position = { 200.0f, 16.0f };
  input.onPointerDown(outside);
  EXPECT_FALSE(provider().has_value()) << "a press outside the selection is not a drag-out";
  input.onPointerUp(outside);
}

TEST(UiWindow, FrameStatsOverlayKeepsAFrameTimeHistory) {
  const auto typeface = graphics::Typeface::fromFile(
    std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf");
  if (typeface == nullptr) {
    GTEST_SKIP() << "the UI test font is not available";
  }
  const auto font = graphics::Font::create(typeface, 14.0f);
  ASSERT_NE(font, nullptr);

  FrameStatsOverlayOptions options;
  options.historyFrames = 4;
  options.historyHeight = 20.0f;
  FrameStatsOverlay overlay(font, Color::White(), options);
  ASSERT_EQ(overlay.historySampleCount(), 4u) << "the ring is sized from the option";

  double peak = 0.0;
  for (int frame = 1; frame <= 4; ++frame) {
    FrameStatistics statistics;
    statistics.lastTotalMilliseconds = static_cast<double>(frame) * 2.0;
    overlay.setStatistics(statistics);
    peak = static_cast<double>(frame) * 2.0;
  }
  EXPECT_EQ(overlay.historySampleCount(), 4u);
  EXPECT_DOUBLE_EQ(overlay.historyLastMilliseconds(), peak) << "the newest frame is the last sample";
  EXPECT_DOUBLE_EQ(overlay.historyPeakMilliseconds(), peak) << "the sparkline scales to its own peak";

  // The graph is a child of the column, so hiding the overlay hides it with the text.
  overlay.setEnabled(false);
  EXPECT_FALSE(overlay.enabled());

  // The graph is opt-in for products that want only the text lines.
  FrameStatsOverlayOptions noGraph;
  noGraph.historyFrames = 0;
  FrameStatsOverlay plain(font, Color::White(), noGraph);
  EXPECT_EQ(plain.historySampleCount(), 0u);
}

TEST(UiWindow, CatalogDirectoryLoadsTheFallbackChain) {
  using namespace coral::ui;
namespace dsx = coral::ui::dsl::experimental;
  namespace fs = std::filesystem;
  const std::string directory = (fs::current_path() /
    ("coral-i18n-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    .string();
  fs::create_directories(directory);
  struct RemoveOnExit {
    std::string directory;
    ~RemoveOnExit() {
      std::error_code ignored;
      fs::remove_all(directory, ignored);
    }
  };
  const RemoveOnExit guard{ directory };
  const auto writeCatalog = [&directory](const std::string& tag, const std::string& text) {
    std::ofstream file(directory + "/" + tag + ".catalog", std::ios::trunc);
    file << text;
  };
  writeCatalog("en", "title=Editor\nstatus=Ready\nshared=Base\n");
  writeCatalog("zh", "title=Editor zh\n");
  writeCatalog("zh-CN", "shared=Locale\n");

  // zh-CN: en, then zh, then zh-CN; the regional file wins for `shared`, the language file for `title`.
  {
    Translator translator(Locale::parse("zh-CN"));
    std::string error;
    ASSERT_TRUE(loadCatalogDirectory(translator, directory, &error)) << error;
    EXPECT_EQ(translator.translate("title"), "Editor zh");
    EXPECT_EQ(translator.translate("shared"), "Locale");
    EXPECT_EQ(translator.translate("status"), "Ready") << "the base catalog fills the gaps";
  }
  // A language with no catalog still gets the base, and the return value says so.
  {
    Translator translator(Locale::parse("fr"));
    std::string error;
    ASSERT_TRUE(loadCatalogDirectory(translator, directory, &error)) << error;
    EXPECT_EQ(translator.translate("status"), "Ready");
  }
  // A directory with no catalogs at all reports the paths it tried.
  {
    Translator translator(Locale::parse("zh-CN"));
    std::string error;
    EXPECT_FALSE(loadCatalogDirectory(translator, directory + "/missing", &error));
    EXPECT_NE(error.find("no catalog found"), std::string::npos) << error;
  }
}

TEST(UiWindow, TranslationsLayerAndInterpolate) {
  using namespace coral::ui;
namespace dsx = coral::ui::dsl::experimental;
  Translator translator(Locale::parse("zh-CN"));
  std::string error;
  // Base catalog, then the locale catalog on top: a key the locale does not translate keeps the base.
  ASSERT_TRUE(translator.addCatalog("base.only=Base text\nshared.title=Base title\n", &error)) << error;
  ASSERT_TRUE(translator.addCatalog("shared.title=Editor zh\n", &error)) << error;
  EXPECT_EQ(translator.translate("shared.title"), "Editor zh") << "the later catalog wins";
  EXPECT_EQ(translator.translate("base.only"), "Base text") << "the base stays for untranslated keys";

  ASSERT_TRUE(translator.addCatalog(
    "greeting=Open {file} in {app}\n"
    "clips.one={0} clip in {file}\n"
    "clips.other={0} clips in {file}\n"));
  EXPECT_EQ(translator.translate("greeting", { { "file", "a.mov" }, { "app", "Coral" } }),
    "Open a.mov in Coral");
  EXPECT_EQ(translator.translate("greeting", { { "file", "a.mov" } }),
    "Open a.mov in {app}") << "a missing argument stays visible";
  EXPECT_EQ(translator.translatePlural("clips", 3, { { "file", "a.mov" } }), "3 clips in a.mov");
  // The plural category comes from the locale: zh has no singular form, en does.
  translator.setLocale(Locale::parse("en-US"));
  EXPECT_EQ(translator.translatePlural("clips", 1, { { "file", "b.mov" } }), "1 clip in b.mov");

  // load() replaces instead of merging; clearCatalog() drops everything.
  ASSERT_TRUE(translator.load("only.key=Only\n"));
  EXPECT_EQ(translator.size(), 1u);
  EXPECT_FALSE(translator.has("shared.title"));
  translator.clearCatalog();
  EXPECT_EQ(translator.translate("only.key"), "only.key");
}

TEST(UiWindow, LocaleDrivesNumberAndDateFormats) {
  using namespace coral::ui;
namespace dsx = coral::ui::dsl::experimental;
  const auto formatNumberIn = [](const char* tag, const double value) {
    return formatNumber(value, numberFormatForLocale(Locale::parse(tag)));
  };
  EXPECT_EQ(formatNumberIn("en-US", 1234.5), "1,234.5");
  EXPECT_EQ(formatNumberIn("de-DE", 1234.5), "1.234,5");
  EXPECT_EQ(formatNumberIn("fr-FR", 1234.5), "1\xE2\x80\xAF" "234,5");
  EXPECT_EQ(formatNumberIn("ru-RU", 1234.5), "1\xC2\xA0" "234,5");
  EXPECT_EQ(formatNumberIn("es-ES", 1234.5), "1.234,5");
  EXPECT_EQ(formatNumberIn("zh-CN", 1234.5), "1,234.5");
  EXPECT_EQ(formatNumberIn("xx-XX", 1234.5), "1,234.5") << "an unknown language falls back to en-US";

  const auto formatDateIn = [](const char* tag) {
    return formatDate(std::chrono::year_month_day{ std::chrono::year{ 2026 },
      std::chrono::November, std::chrono::day{ 16 } }, dateFormatForLocale(Locale::parse(tag)));
  };
  EXPECT_EQ(formatDateIn("en-US"), "11/16/2026");
  EXPECT_EQ(formatDateIn("en-GB"), "16/11/2026");
  EXPECT_EQ(formatDateIn("de-DE"), "16.11.2026");
  EXPECT_EQ(formatDateIn("fr-FR"), "16/11/2026");
  EXPECT_EQ(formatDateIn("zh-CN"), "2026-11-16");
  EXPECT_EQ(formatDateIn("ja-JP"), "2026-11-16");
}

TEST(UiWindow, TranslationCatalogAndPluralRules) {
  using namespace coral::ui;
namespace dsx = coral::ui::dsl::experimental;
  Translator translator(Locale::parse("en-US"));
  std::string error;
  ASSERT_TRUE(translator.load(
    "# sample\n"
    "editor.title=Coral Editor\n"
    "editor.unsaved=Unsaved\\nchanges\n"
    "timeline.clips.one={0} clip\n"
    "timeline.clips.other={0} clips\n",
    &error)) << error;
  EXPECT_EQ(translator.size(), 4u);
  EXPECT_EQ(translator.translate("editor.title"), "Coral Editor");
  EXPECT_EQ(translator.translate("editor.unsaved"), "Unsaved\nchanges");
  EXPECT_EQ(translator.translate("missing.key"), "missing.key") << "a missing key stays visible";
  EXPECT_EQ(translator.translatePlural("timeline.clips", 1), "1 clip");
  EXPECT_EQ(translator.translatePlural("timeline.clips", 3), "3 clips");

  // The plural category is the locale's, not the catalog's.
  EXPECT_EQ(pluralCategory(Locale::parse("en"), 1), PluralCategory::One);
  EXPECT_EQ(pluralCategory(Locale::parse("en"), 0), PluralCategory::Other);
  EXPECT_EQ(pluralCategory(Locale::parse("fr"), 0), PluralCategory::One);
  EXPECT_EQ(pluralCategory(Locale::parse("zh-CN"), 1), PluralCategory::Other);
  EXPECT_EQ(pluralCategory(Locale::parse("ru"), 21), PluralCategory::One);
  EXPECT_EQ(pluralCategory(Locale::parse("ru"), 3), PluralCategory::Few);
  EXPECT_EQ(pluralCategory(Locale::parse("ru"), 11), PluralCategory::Many);
  EXPECT_EQ(pluralCategory(Locale::parse("ru"), 1.5), PluralCategory::Other);
  EXPECT_EQ(pluralCategory(Locale::parse("pl"), 2), PluralCategory::Few);
  EXPECT_EQ(pluralCategory(Locale::parse("pl"), 5), PluralCategory::Many);
  EXPECT_EQ(pluralCategory(Locale::parse("ar"), 0), PluralCategory::Zero);
  EXPECT_EQ(pluralCategory(Locale::parse("ar"), 2), PluralCategory::Two);
  EXPECT_EQ(pluralCategory(Locale::parse("ar"), 7), PluralCategory::Few);
  EXPECT_EQ(pluralCategory(Locale::parse("ar"), 42), PluralCategory::Many);

  // A malformed catalog is rejected without replacing the working one.
  EXPECT_FALSE(translator.load("broken line without separator\n"));
  EXPECT_EQ(translator.translate("editor.title"), "Coral Editor") << "the old catalog survives";

  // Locale tags round-trip and ignore the script subtag.
  EXPECT_EQ(Locale::parse("zh-Hans-CN").language, "zh");
  EXPECT_EQ(Locale::parse("zh-Hans-CN").region, "CN");
  EXPECT_EQ(Locale::parse("es-419").tag(), "es-419");
  EXPECT_EQ(Locale::parse("en-us").tag(), "en-US");

  // A translator with no catalog still answers, with the key.
  Translator empty;
  EXPECT_EQ(empty.translatePlural("anything", 2), "anything");
}

TEST(UiWindow, AChildChangePatchesOneSegmentPerLevel) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  const float previousLimit = Layer::recordingCacheAreaLimit();
  const std::size_t previousBudget = Layer::recordingCacheBudgetBytes();
  struct RestoreCacheSettings {
    float limit;
    std::size_t budget;
    ~RestoreCacheSettings() {
      Layer::setRecordingCacheAreaLimit(limit);
      Layer::setRecordingCacheBudgetBytes(budget);
    }
  };
  const RestoreCacheSettings restore{ previousLimit, previousBudget };
  Layer::setRecordingCacheAreaLimit(0.0f);
  Layer::setRecordingCacheBudgetBytes(64u * 1024u * 1024u);

  Window::Options options;
  options.headless = true;
  options.width = 320;
  options.height = 240;
  options.frameLimit = 0;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  auto root = std::make_shared<Flex>(Axis::Column);
  auto row = std::make_shared<Flex>(Axis::Row);
  auto swatch = std::make_shared<ColorLayer>(Color::MakeRGBA8(40, 90, 140, 255));
  swatch->setBounds(Rect::MakeWH(16.0f, 16.0f));
  row->addChild(swatch);
  auto sibling = std::make_shared<ColorLayer>(Color::MakeRGBA8(70, 70, 70, 255));
  sibling->setBounds(Rect::MakeWH(16.0f, 16.0f));
  row->addChild(sibling);
  root->addChild(row);
  window->setRoot(root);

  ASSERT_TRUE(window->renderFrame());
  const std::size_t entriesBefore = Layer::recordingCacheEntryCount();
  ASSERT_GT(entriesBefore, 0u) << "the row and the root are recorded";
  const std::size_t rebuiltBefore = Layer::recordingSegmentRebuildCount();

  // One deep child repaints: each segmentable ancestor patches exactly the segment that contains it.
  swatch->setBackgroundColor(Color::MakeRGBA8(220, 90, 40, 255));
  ASSERT_TRUE(window->renderFrame());
  // Only the top recording exists here: its child segments are recorded inline (a child inside a scope
  // does not build its own), so exactly one segment is patched for the change.
  const std::size_t rebuilt = Layer::recordingSegmentRebuildCount() - rebuiltBefore;
  EXPECT_EQ(rebuilt, 1u) << "the container patches the segment that holds the changed child";
  EXPECT_EQ(Layer::recordingCacheEntryCount(), entriesBefore)
    << "the ancestor recordings survive a child change";

  // The picture is consistent: both children still paint after the patch.
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap));

  // Drop every local reference: a recording lives with its layer, not with the window.
  window->setRoot(nullptr);
  swatch.reset();
  sibling.reset();
  row.reset();
  root.reset();
  EXPECT_EQ(Layer::recordingCacheEntryCount(), 0u);
}

TEST(UiWindow, RecordingsStayWithinTheMemoryBudget) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  const float previousLimit = Layer::recordingCacheAreaLimit();
  const std::size_t previousBudget = Layer::recordingCacheBudgetBytes();
  struct RestoreCacheSettings {
    float limit;
    std::size_t budget;
    ~RestoreCacheSettings() {
      Layer::setRecordingCacheAreaLimit(limit);
      Layer::setRecordingCacheBudgetBytes(budget);
    }
  };
  const RestoreCacheSettings restore{ previousLimit, previousBudget };

  // No per-subtree cap: the budget is what limits the cache in this test.
  Layer::setRecordingCacheAreaLimit(0.0f);

  const auto buildTree = [](const int panels) {
    auto root = std::make_shared<Flex>(Axis::Column);
    for (int panel = 0; panel < panels; ++panel) {
      auto row = std::make_shared<Flex>(Axis::Row);
      for (int cell = 0; cell < 8; ++cell) {
        auto swatch = std::make_shared<ColorLayer>(
          Color::MakeRGBA8(static_cast<uint8_t>(32 + cell * 8), 64, 96, 255));
        swatch->setBounds(Rect::MakeWH(12.0f, 12.0f));
        row->addChild(swatch);
      }
      root->addChild(row);
    }
    return root;
  };
  const auto runOneFrame = [](const std::shared_ptr<Layer>& root) {
    Window::Options options;
    options.headless = true;
    options.width = 320;
    options.height = 240;
    options.frameLimit = 1;
    auto window = Window::create(options);
    if (window == nullptr) return std::pair<std::size_t, std::size_t>{ 0, 0 };
    window->setRoot(root);
    window->run();
    return std::pair<std::size_t, std::size_t>{
      Layer::recordingCacheEntryCount(), Layer::recordingCacheBytesInUse() };
  };

  Layer::setRecordingCacheBudgetBytes(64u * 1024u * 1024u);
  const auto generous = runOneFrame(buildTree(24));
  EXPECT_GT(generous.first, 0u) << "subtrees without children are recorded once they fit the budget";
  EXPECT_GT(generous.second, 0u);
  EXPECT_LE(generous.second, Layer::recordingCacheBudgetBytes()) << "the budget is a ceiling";

  // A budget that cannot hold the tree must keep the total under it, not exceed it.
  Layer::setRecordingCacheBudgetBytes(2048u);
  const auto tight = runOneFrame(buildTree(24));
  EXPECT_LE(tight.second, 2048u) << "eviction keeps the total inside the budget";
  EXPECT_LE(tight.second, Layer::recordingCacheBudgetBytes()) << "the tight budget is a ceiling too";

  // Destroying the window must return its recordings to the budget, or a long-lived process leaks.
  EXPECT_EQ(Layer::recordingCacheEntryCount(), 0u) << "no recording outlives its layer tree";
  EXPECT_EQ(Layer::recordingCacheBytesInUse(), 0u);
}

TEST(UiWindow, TheWindowLaysOutAnOverlayAboveTheRoot) {
  Window::Options options;
  options.headless = true;
  options.width = 200;
  options.height = 120;
  options.frameLimit = 1;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  // A plain layer root keeps the window bounds; a Flex root would measure to its own content.
  auto root = std::make_shared<Layer>();
  window->setRoot(root);

  // The slot is the layer the product hands over; the panel inside it is positioned absolutely, the way
  // a fixed-size debug panel goes into a corner.
  auto slot = std::make_shared<Flex>(Axis::Column);
  auto panel = std::make_shared<Flex>(Axis::Column);
  LayoutParams params;
  params.position = Position::Absolute;
  params.hasLeft = true;
  params.left = 10.0f;
  params.hasTop = true;
  params.top = 20.0f;
  panel->setLayoutParams(params);
  auto marker = std::make_shared<ColorLayer>(Color::MakeRGBA8(255, 0, 0, 255));
  marker->setBounds(Rect::MakeWH(40.0f, 30.0f));
  panel->addChild(marker);
  panel->setBounds(Rect::MakeWH(80.0f, 50.0f));
  panel->setNeedsLayout();
  slot->addChild(panel);

  window->setOverlay(slot);
  ASSERT_EQ(window->overlay(), slot);
  EXPECT_FALSE(slot->hitTestable()) << "a diagnostic slot must not steal clicks";

  EXPECT_EQ(window->run(), 1);
  EXPECT_FLOAT_EQ(slot->bounds().getWidth(), 200.0f) << "the slot is laid out at the window size";
  EXPECT_LE(panel->bounds().getLeft(), 10.5f);
  EXPECT_GE(panel->bounds().getWidth(), 40.0f) << "the panel keeps its own box inside the slot";

  window->clearOverlay();
  EXPECT_EQ(window->overlay(), nullptr);
  EXPECT_EQ(root->children().size(), 0u) << "the overlay is removed from the root as well";
}

TEST(UiWindow, ThemeTextRoundTripsThroughTheParser) {
  const Theme dark = Theme::dark();
  std::string error;
  const std::optional<Theme> parsed = parseTheme(formatTheme(dark), Theme::dark(), &error);
  ASSERT_TRUE(parsed.has_value()) << error;
  EXPECT_EQ(parsed->background.toR8G8B8A8(), dark.background.toR8G8B8A8());
  EXPECT_EQ(parsed->surface.toR8G8B8A8(), dark.surface.toR8G8B8A8());
  EXPECT_EQ(parsed->controls.accent.toR8G8B8A8(), dark.controls.accent.toR8G8B8A8());
  EXPECT_EQ(parsed->controls.knob.toR8G8B8A8(), dark.controls.knob.toR8G8B8A8());
  EXPECT_FLOAT_EQ(parsed->focusRingWidth, dark.focusRingWidth);

  // The parser is a development tool, so it fails loudly instead of ignoring what it did not understand.
  EXPECT_FALSE(parseTheme("background=nope\n").has_value());
  std::string keyError;
  const auto unknown = parseTheme("background=#FFFFFF\nnotAKey=#000000\n", Theme::dark(), &keyError);
  EXPECT_FALSE(unknown.has_value());
  EXPECT_NE(keyError.find("unknown key"), std::string::npos) << keyError;
}

TEST(UiWindow, ThemeHotReloaderKeepsTheLastGoodTheme) {
  struct RemoveOnExit {
    std::string path;
    ~RemoveOnExit() {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  };
  const std::string path = (std::filesystem::current_path() /
    ("coral-theme-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
      ".theme")).string();
  const RemoveOnExit guard{ path };
  const auto writeFile = [&path](const std::string& text) {
    std::ofstream file(path, std::ios::trunc);
    file << text;
  };

  writeFile("accent=#112233\n");
  ThemeReloadOptions options;
  options.pollInterval = std::chrono::milliseconds(0);
  ThemeHotReloader reloader(path, options);
  EXPECT_EQ(reloader.reloadCount(), 1u) << "construction loads the file";
  EXPECT_EQ(reloader.theme().accent.toR8G8B8A8()[0], 0x11) << "the accent is the file's";
  EXPECT_TRUE(reloader.lastError().empty());

  writeFile("accent=#445566\nsurface=#010203\n");
  reloader.poll();
  EXPECT_EQ(reloader.reloadCount(), 2u);
  EXPECT_EQ(reloader.theme().accent.toR8G8B8A8()[1], 0x55);
  EXPECT_EQ(reloader.theme().surface.toR8G8B8A8()[0], 0x01);

  // A broken file keeps the theme that is on screen and reports the line.
  writeFile("accent=gggggg\n");
  reloader.poll();
  EXPECT_EQ(reloader.reloadCount(), 2u);
  EXPECT_FALSE(reloader.lastError().empty());
  EXPECT_EQ(reloader.theme().accent.toR8G8B8A8()[1], 0x55) << "the last good theme stays";

  // Attaching applies the theme and detaching stops the polling.
  Window::Options windowOptions;
  windowOptions.headless = true;
  windowOptions.width = 64;
  windowOptions.height = 64;
  auto window = Window::create(windowOptions);
  if (window != nullptr) {
    reloader.attach(*window);
    reloader.detach();
  }
}

TEST(UiWindow, SpecHotReloaderReloadsOnlyWhenTheFileChanges) {
  struct RemoveOnExit {
    std::string path;
    ~RemoveOnExit() {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
  };
  const std::string path = (std::filesystem::current_path() /
    ("coral-hot-reload-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".spec")).string();
  const RemoveOnExit guard{ path };
  const auto writeFile = [&path](const std::string& text) {
    std::ofstream file(path, std::ios::trunc);
    file << text;
  };

  std::string error;
  int reloaded = 0;
  SpecReloadOptions options;
  options.pollInterval = std::chrono::milliseconds(0);
  options.onError = [&error](const std::string& message) { error = message; };
  options.onReloaded = [&reloaded] { ++reloaded; };

  writeFile("Rect#root width=40 height=20");
  SpecHotReloader reloader(path, ElementRegistry::shared(), options);
  EXPECT_EQ(reloader.reloadCount(), 1u) << "construction loads the current file";
  EXPECT_EQ(reloaded, 1);
  EXPECT_TRUE(reloader.lastError().empty());
  const ElementPtr first = reloader.build();
  ASSERT_NE(first, nullptr);

  reloader.poll();
  EXPECT_EQ(reloader.reloadCount(), 1u) << "an unchanged file is not re-parsed";

  // The size differs as well, so the change is visible even on a coarse file timestamp.
  writeFile("Rect#root width=80 height=40 radius=2");
  reloader.poll();
  EXPECT_EQ(reloader.reloadCount(), 2u);
  EXPECT_EQ(reloaded, 2);
  const ElementPtr second = reloader.build();
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first.get(), second.get()) << "a successful reload builds a new tree";

  // A broken spec keeps the previous UI and reports why.
  writeFile("Rect#root width=oops");
  reloader.poll();
  EXPECT_EQ(reloader.reloadCount(), 2u);
  EXPECT_EQ(error.find("invalid element spec"), 0u) << error;
  EXPECT_EQ(reloader.build().get(), second.get()) << "the last good tree stays mounted";

  // Disabling the watcher stops the polling entirely.
  writeFile("Rect#root width=120 height=60");
  reloader.setEnabled(false);
  reloader.poll();
  EXPECT_EQ(reloader.reloadCount(), 2u);
  reloader.setEnabled(true);
  reloader.poll();
  EXPECT_EQ(reloader.reloadCount(), 3u);

  // A missing file still yields a mountable placeholder.
  const std::string missing = path + ".missing";
  SpecHotReloader placeholder(missing);
  EXPECT_NE(placeholder.build(), nullptr) << "a missing spec mounts an empty layer";
  EXPECT_FALSE(placeholder.lastError().empty());
}

TEST(UiWindow, CloseRequestRunsTheInterceptor) {
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 24;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);

  int calls = 0;
  bool allow = false;
  window->setCloseRequestedCallback([&] {
    ++calls;
    return allow;
  });
  window->requestClose();
  EXPECT_TRUE(window->running()) << "a vetoed close keeps the window alive";
  EXPECT_EQ(calls, 1);

  allow = true;
  window->requestClose();
  EXPECT_FALSE(window->running());
  EXPECT_EQ(calls, 2);
}

TEST(UiApplication, ClosedWindowIsAliveDuringItsCallback) {
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 48;
  options.frameLimit = 1;
  std::shared_ptr<Window> window = application->createWindow(options);
  ASSERT_NE(window, nullptr);

  bool called = false;
  int closedWidth = 0;
  application->setOnWindowClosed([&](Window* closed) {
    called = true;
    closedWidth = closed->width();
    // Reading window state here must be safe: the application keeps the window alive for the call.
    (void)closed->diagnostics();
    (void)closed->title();
  });
  window->requestClose();
  application->run();
  EXPECT_TRUE(called);
  EXPECT_EQ(closedWidth, 64);
}

TEST(UiApplication, ThreeThreadedWindowsShareOneHost) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 24;
  options.height = 18;
  options.renderHost = RenderHostKind::RenderThread;

  const uint8_t expected[3][3] = {
    { 200, 40, 70 },
    { 40, 200, 70 },
    { 40, 70, 200 },
  };
  std::vector<std::shared_ptr<Window>> windows;
  for (const auto& color: expected) {
    std::shared_ptr<Window> window = application->createWindow(options);
    ASSERT_NE(window, nullptr) << Window::lastCreationError();
    window->setRoot(std::make_shared<ColorLayer>(
      Color::MakeRGBA8(color[0], color[1], color[2], 255)));
    ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
    windows.push_back(std::move(window));
  }
  ASSERT_EQ(windows.size(), 3u);
  EXPECT_EQ(application->openWindowCount(), 3u);
  ASSERT_NE(application->renderHost(), nullptr);
  EXPECT_EQ(application->renderHost()->targetCount(), 3u);

  for (size_t i = 0; i < windows.size(); ++i) {
    Bitmap bitmap;
    ASSERT_TRUE(windows[i]->snapshot(&bitmap)) << windows[i]->diagnostics();
    const uint32_t pixel = bitmap.makePixmap().getPixelARGB32(12, 9);
    EXPECT_EQ((pixel >> 16) & 0xFFu, expected[i][0]) << "window " << i;
    EXPECT_EQ((pixel >> 8) & 0xFFu, expected[i][1]) << "window " << i;
    EXPECT_EQ(pixel & 0xFFu, expected[i][2]) << "window " << i;
  }

  for (const std::shared_ptr<Window>& window: windows) window->requestClose();
  application->run();
  EXPECT_EQ(application->openWindowCount(), 0u);
}

TEST(UiApplication, ThreeWindowsKeepIndependentScaleAndTheme) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  auto application = Application::create();
  const float scales[3] = { 1.0f, 1.5f, 2.0f };
  std::vector<std::shared_ptr<Window>> windows;
  for (size_t i = 0; i < 3; ++i) {
    Window::Options options;
    options.headless = true;
    options.width = 120;
    options.height = 90;
    options.renderHost = RenderHostKind::RenderThread;
    options.uiScale = scales[i];
    std::shared_ptr<Window> window = application->createWindow(options);
    ASSERT_NE(window, nullptr) << Window::lastCreationError();
    window->setTheme(i == 1 ? Theme::light() : Theme::dark());
    window->setRoot(std::make_shared<ColorLayer>(
      Color::MakeRGBA8(static_cast<uint8_t>(40 + i * 60), 90, 160, 255)));
    ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
    windows.push_back(std::move(window));
  }
  ASSERT_EQ(windows.size(), 3u);
  for (size_t i = 0; i < windows.size(); ++i) {
    EXPECT_FLOAT_EQ(windows[i]->uiScale(), scales[i]);
    Bitmap bitmap;
    ASSERT_TRUE(windows[i]->snapshot(&bitmap)) << windows[i]->diagnostics();
    EXPECT_EQ(bitmap.width(), 120);
    EXPECT_EQ(bitmap.height(), 90);
    const uint32_t pixel = bitmap.makePixmap().getPixelARGB32(60, 45);
    EXPECT_EQ((pixel >> 16) & 0xFFu, 40u + i * 60u) << "window " << i;
  }
  EXPECT_EQ(application->renderHost()->targetCount(), 3u);
  for (const std::shared_ptr<Window>& window: windows) window->requestClose();
  application->run();
}

TEST(UiApplication, CloseAllHonoursPerWindowVeto) {
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 40;
  options.height = 30;
  std::shared_ptr<Window> first = application->createWindow(options);
  std::shared_ptr<Window> second = application->createWindow(options);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  first->setCloseRequestedCallback([] { return false; });
  EXPECT_EQ(application->closeAllWindows(), 2u);
  EXPECT_TRUE(first->running()) << "a veto keeps its window open";
  EXPECT_FALSE(second->running());

  first->setCloseRequestedCallback({});
  EXPECT_EQ(application->closeAllWindows(), 1u);
  application->run();
  EXPECT_EQ(application->openWindowCount(), 0u);
}

TEST(UiApplication, CapturesAndAppliesWindowLayout) {
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 100;
  options.height = 80;
  std::shared_ptr<Window> first = application->createWindow(options);
  ASSERT_NE(first, nullptr);
  first->setTitle("first");
  options.width = 200;
  options.height = 150;
  std::shared_ptr<Window> second = application->createWindow(options);
  ASSERT_NE(second, nullptr);
  second->setTitle("second");

  const std::vector<WindowLayout> layout = application->captureWindowLayout();
  ASSERT_EQ(layout.size(), 2u);
  EXPECT_EQ(layout[0].title, "first");
  EXPECT_EQ(layout[0].bounds.getWidth(), 100.0f);
  EXPECT_EQ(layout[1].bounds.getHeight(), 150.0f);

  std::vector<WindowLayout> restored = layout;
  restored[0].bounds = Rect::MakeLTRB(10.0f, 20.0f, 330.0f, 260.0f);
  restored[0].fullscreen = true;
  ASSERT_TRUE(application->applyWindowLayout(restored));
  EXPECT_EQ(first->width(), 320);
  EXPECT_EQ(first->height(), 240);
  EXPECT_TRUE(first->isFullscreen());
  EXPECT_FALSE(second->isFullscreen());

  application->activateWindow(*second); // headless/no-activation platforms treat this as a no-op
  std::vector<WindowLayout> mismatched = restored;
  mismatched.pop_back();
  EXPECT_FALSE(application->applyWindowLayout(mismatched));

  const std::filesystem::path layoutPath =
    std::filesystem::temp_directory_path() / "coral-ui-window-layout.txt";
  ASSERT_TRUE(application->saveWindowLayout(layoutPath.string()));
  first->restoreGeometry(Rect::MakeLTRB(0.0f, 0.0f, 100.0f, 80.0f));
  first->setFullscreen(false);
  ASSERT_TRUE(application->loadWindowLayout(layoutPath.string()));
  EXPECT_EQ(first->bounds().getWidth(), 320.0f);
  EXPECT_TRUE(first->isFullscreen());
  std::error_code removeError;
  std::filesystem::remove(layoutPath, removeError);

  first->requestClose();
  second->requestClose();
  application->run();
}

#if defined(_WIN32)
TEST(UiWindow, AThreadedNativeWindowRendersThroughItsRenderThread) {
  Window::Options options;
  options.headless = false;
  options.width = 96;
  options.height = 64;
  options.renderHost = RenderHostKind::RenderThread;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "a threaded native window could not be created in this session";
  }
  EXPECT_EQ(window->rendererKind(), RendererKind::GPU);
  EXPECT_TRUE(window->isRenderTargetOnScreen())
    << "the threaded Win32 target must present into the HWND, not an offscreen surface";
  window->setTitle("Coral UI threaded");
  EXPECT_EQ(window->title(), "Coral UI threaded");
  EXPECT_GT(window->bounds().getWidth(), 0.0f);
  window->setMinimumSize(120, 80);
  EXPECT_EQ(window->minimumWidth(), 120);
  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(210, 70, 30, 255)));
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap)) << window->diagnostics();
  const auto pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  const uint32_t pixel = pixels.getPixelARGB32(48, 32);
  EXPECT_EQ((pixel >> 16) & 0xFFu, 210u);
  EXPECT_EQ((pixel >> 8) & 0xFFu, 70u);
  EXPECT_EQ(pixel & 0xFFu, 30u);

  ASSERT_TRUE(window->resize(240, 160)) << window->diagnostics();
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap resized;
  ASSERT_TRUE(window->snapshot(&resized)) << window->diagnostics();
  EXPECT_EQ(resized.width(), window->width());
  EXPECT_EQ(resized.height(), window->height());
}
#else
TEST(UiWindow, AThreadedX11WindowRendersThroughItsRenderThread) {
  Window::Options options;
  options.headless = false;
  options.width = 96;
  options.height = 64;
  options.platform = Window::NativePlatform::X11;
  options.renderHost = RenderHostKind::RenderThread;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no X11 session or threaded X11 target is available";
  }
  EXPECT_EQ(window->rendererKind(), RendererKind::GPU);
  EXPECT_TRUE(window->isRenderTargetOnScreen())
    << "the threaded X11 target must create a surface on the X11 window, not an offscreen one";
  window->setTitle("Coral UI X11 threaded");
  EXPECT_EQ(window->title(), "Coral UI X11 threaded");
  window->setMinimumSize(120, 80);
  EXPECT_EQ(window->minimumWidth(), 120);
  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(30, 120, 210, 255)));
  ASSERT_TRUE(window->renderFrame()) << window->diagnostics();
  graphics::Bitmap bitmap;
  ASSERT_TRUE(window->snapshot(&bitmap)) << window->diagnostics();
  const auto pixels = bitmap.makePixmap();
  ASSERT_TRUE(pixels.isValid());
  const uint32_t pixel = pixels.getPixelARGB32(bitmap.width() / 2, bitmap.height() / 2);
  EXPECT_EQ((pixel >> 16) & 0xFFu, 30u);
  EXPECT_EQ((pixel >> 8) & 0xFFu, 120u);
  EXPECT_EQ(pixel & 0xFFu, 210u);
}

TEST(UiWindow, AThreadedWaylandWindowIsRejectedWithAReason) {
  if (std::getenv("WAYLAND_DISPLAY") == nullptr) {
    GTEST_SKIP() << "no Wayland session";
  }
  Window::Options options;
  options.headless = false;
  options.width = 96;
  options.height = 64;
  options.platform = Window::NativePlatform::Wayland;
  options.renderHost = RenderHostKind::RenderThread;
  auto window = Window::create(options);
  EXPECT_EQ(window, nullptr);
  EXPECT_NE(Window::lastCreationError().find("threaded Wayland"), std::string::npos)
    << Window::lastCreationError();
}
#endif

TEST(UiWindow, AnApplicationSharesOneRenderHostBetweenThreadedWindows) {
  CORAL_REQUIRE_RECORDING_BACKEND();
  auto application = Application::create();
  Window::Options options;
  options.headless = true;
  options.width = 16;
  options.height = 12;
  options.renderHost = RenderHostKind::RenderThread;
  auto first = application->createWindow(options);
  auto second = application->createWindow(options);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(application->renderHost(), nullptr);
  EXPECT_EQ(application->renderHost()->targetCount(), 2u);

  first->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(180, 20, 40, 255)));
  second->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 180, 40, 255)));
  ASSERT_TRUE(first->renderFrame()) << first->diagnostics();
  ASSERT_TRUE(second->renderFrame()) << second->diagnostics();

  graphics::Bitmap firstPixels;
  graphics::Bitmap secondPixels;
  ASSERT_TRUE(first->snapshot(&firstPixels)) << first->diagnostics();
  ASSERT_TRUE(second->snapshot(&secondPixels)) << second->diagnostics();
  EXPECT_EQ((firstPixels.makePixmap().getPixelARGB32(8, 6) >> 16) & 0xFFu, 180u);
  EXPECT_EQ((secondPixels.makePixmap().getPixelARGB32(8, 6) >> 8) & 0xFFu, 180u);

  ASSERT_TRUE(application->recoverRenderDevice());
  EXPECT_EQ(first->recoveryCount(), 1);
  EXPECT_EQ(second->recoveryCount(), 1);
  ASSERT_TRUE(first->renderFrame()) << first->diagnostics();
  ASSERT_TRUE(second->renderFrame()) << second->diagnostics();
}

TEST(UiWindow, AnApplicationCanOwnARasterWindow) {
  auto application = Application::create();
  ASSERT_NE(application, nullptr);
  Window::Options options;
  options.width = 6;
  options.height = 4;
  options.rendererBackend = RendererBackend::Raster;
  auto window = application->createWindow(options);
  ASSERT_NE(window, nullptr);
  EXPECT_EQ(window->rendererKind(), RendererKind::Raster);
  EXPECT_EQ(window->rendererDevice(), nullptr);
  EXPECT_TRUE(window->renderFrame());
}

} // namespace


TEST(UiWindow, MultipleFrameCallbacksRunAndCanBeRemoved) {
  Window::Options options;
  options.headless = true;
  options.width = 32;
  options.height = 24;
  auto window = Window::create(options);
  ASSERT_NE(window, nullptr);
  int first = 0;
  int second = 0;
  const uint64_t firstId = window->addFrameCallback([&first] { ++first; });
  const uint64_t secondId = window->addFrameCallback([&second] { ++second; });
  EXPECT_NE(firstId, 0u);
  EXPECT_NE(firstId, secondId);

  ASSERT_TRUE(window->renderFrame());
  EXPECT_EQ(first, 1);
  EXPECT_EQ(second, 1);

  window->removeFrameCallback(firstId);
  ASSERT_TRUE(window->renderFrame());
  EXPECT_EQ(first, 1) << "a removed observer must not run again";
  EXPECT_EQ(second, 2);

  window->removeFrameCallback(secondId);
  ASSERT_TRUE(window->renderFrame());
  EXPECT_EQ(first, 1);
  EXPECT_EQ(second, 2);
}

namespace {

/** Counts the pointer callbacks a hovered leaf receives. */
class HoverProbe final : public Layer {
 public:
  bool onPointerMove(const PointerEvent&) override {
    ++moves;
    return false;
  }

  bool onPointerLeave() override {
    ++leaves;
    return false;
  }

  int moves = 0;
  int leaves = 0;
};

} // namespace

TEST(UiWindow, LeavingTheWindowClearsTheHoveredLayer) {
  Window::Options options;
  options.headless = true;
  options.width = 64;
  options.height = 64;
  auto window = Window::create(options);
  if (window == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }

  auto root = std::make_shared<Layer>();
  auto probe = std::make_shared<HoverProbe>();
  probe->setBounds(Rect::MakeWH(20.0f, 20.0f));
  root->addChild(probe);
  window->setRoot(root);

  PointerEvent inside;
  inside.position = { 10.0f, 10.0f };
  window->pointerMove(inside);
  EXPECT_EQ(probe->moves, 1);
  EXPECT_EQ(probe->leaves, 0);

  // The platform reports "the pointer left the window" by setting inside=false; the window must clear
  // hover instead of leaving the last target highlighted.
  PointerEvent outside;
  outside.position = { 10.0f, 10.0f };
  outside.inside = false;
  window->pointerMove(outside);
  EXPECT_EQ(probe->leaves, 1);

  window->pointerMove(outside);
  EXPECT_EQ(probe->leaves, 1) << "leaving twice must not notify twice";
}

namespace {

std::shared_ptr<Window> shellWindow() {
  Window::Options options;
  options.headless = true;
  options.width = 160;
  options.height = 90;
  auto window = Window::create(options);
  if (window == nullptr) return nullptr;
  return window;
}

} // namespace

TEST(UiWindow, ShellStateTracksMaximizeRestoreAndMinimize) {
  auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  EXPECT_FALSE(window->isMaximized());
  EXPECT_FALSE(window->isMinimized());

  window->maximize();
  EXPECT_TRUE(window->isMaximized());
  window->minimize();
  EXPECT_TRUE(window->isMinimized());
  window->restore();
  EXPECT_FALSE(window->isMaximized());
  EXPECT_FALSE(window->isMinimized());
}

TEST(UiWindow, ShellPropertiesClampAndReport) {
  auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  window->setOpacity(1.5f);
  EXPECT_FLOAT_EQ(window->opacity(), 1.0f);
  window->setOpacity(-0.5f);
  EXPECT_FLOAT_EQ(window->opacity(), 0.0f);

  window->setAlwaysOnTop(true);
  EXPECT_TRUE(window->isAlwaysOnTop());
  window->setCursor(Window::Cursor::Hand);
  EXPECT_EQ(window->cursor(), Window::Cursor::Hand);

  auto parent = shellWindow();
  if (parent != nullptr) {
    window->setTransientFor(parent.get());
    EXPECT_EQ(window->transientFor(), parent.get());
  }
  window->setModal(true);
  EXPECT_TRUE(window->isModal());
  window->setModal(false);
  EXPECT_FALSE(window->isModal());
}

TEST(UiShell, WindowManagerActivatesAndBlocksModalTransients) {
  auto editor = shellWindow();
  auto dialog = shellWindow();
  if (editor == nullptr || dialog == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  WindowManager manager;
  manager.add(editor);
  manager.add(dialog);
  EXPECT_EQ(manager.active(), editor.get());
  ASSERT_TRUE(manager.activate(dialog.get()));
  EXPECT_EQ(manager.active(), dialog.get());

  dialog->setTransientFor(editor.get());
  dialog->setModal(true);
  EXPECT_TRUE(manager.isBlocked(*editor)) << "a modal transient blocks its parent";
  EXPECT_FALSE(manager.activate(editor.get()));
  EXPECT_EQ(manager.active(), dialog.get()) << "the blocked activation is a no-op";

  dialog->setModal(false);
  EXPECT_FALSE(manager.isBlocked(*editor));
  EXPECT_TRUE(manager.activate(editor.get()));
  EXPECT_EQ(manager.active(), editor.get());

  manager.remove(dialog.get());
  EXPECT_EQ(manager.windows().size(), 1u);
}

TEST(UiShell, WindowManagerArrangesAndPersistsGeometry) {
  auto first = shellWindow();
  auto second = shellWindow();
  auto third = shellWindow();
  if (first == nullptr || second == nullptr || third == nullptr) {
    GTEST_SKIP() << "no GPU device is available for the headless surface";
  }
  WindowManager manager;
  manager.add(first);
  manager.add(second);
  manager.add(third);

  const Rect area = Rect::MakeWH(900.0f, 400.0f);
  const std::vector<Rect> tiles = manager.arrange(WindowArrange::TileHorizontal, area, 10.0f);
  ASSERT_EQ(tiles.size(), 3u);
  EXPECT_FLOAT_EQ(tiles[0].getWidth(), (900.0f - 20.0f) / 3.0f);
  EXPECT_FLOAT_EQ(tiles[1].getLeft(), tiles[0].getRight() + 10.0f);
  EXPECT_FLOAT_EQ(tiles[2].getRight(), area.getRight());
  for (const Rect& tile: tiles) {
    EXPECT_GE(tile.getLeft(), area.getLeft());
    EXPECT_LE(tile.getBottom(), area.getBottom());
  }

  WindowGeometry geometry;
  geometry.x = 12;
  geometry.y = 34;
  geometry.width = 800;
  geometry.height = 600;
  geometry.maximized = true;
  manager.setGeometry("editor", geometry);
  const std::string saved = manager.save();
  EXPECT_FALSE(saved.empty());

  WindowManager restored;
  ASSERT_TRUE(restored.load(saved));
  const auto loaded = restored.geometry("editor");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->x, 12);
  EXPECT_EQ(loaded->y, 34);
  EXPECT_EQ(loaded->width, 800);
  EXPECT_EQ(loaded->height, 600);
  EXPECT_TRUE(loaded->maximized);

  EXPECT_FALSE(restored.load("not a geometry record"));
}

TEST(UiWindow, ClientDecorationsHitTestResizeBordersCaptionsAndButtons) {
  auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  window->setDecorations(Window::Decorations::Client);
  EXPECT_EQ(window->decorations(), Window::Decorations::Client);

  WindowChromeMetrics metrics;
  metrics.titleBarHeight = 30.0f;
  metrics.resizeBorder = 5.0f;
  metrics.buttonWidth = 40.0f;
  window->setChromeMetrics(metrics);

  // The headless window is 160x90: at 40pt per button the three buttons cover 40..160.
  EXPECT_EQ(window->hitTest(Point{ 10.0f, 15.0f }), WindowHit::Caption);
  EXPECT_EQ(window->hitTest(Point{ 80.0f, 45.0f }), WindowHit::Client);
  EXPECT_EQ(window->hitTest(Point{ 2.0f, 45.0f }), WindowHit::ResizeLeft);
  EXPECT_EQ(window->hitTest(Point{ 157.0f, 45.0f }), WindowHit::ResizeRight);
  EXPECT_EQ(window->hitTest(Point{ 80.0f, 2.0f }), WindowHit::ResizeTop);
  EXPECT_EQ(window->hitTest(Point{ 80.0f, 87.0f }), WindowHit::ResizeBottom);
  EXPECT_EQ(window->hitTest(Point{ 2.0f, 2.0f }), WindowHit::ResizeTopLeft);
  EXPECT_EQ(window->hitTest(Point{ 157.0f, 87.0f }), WindowHit::ResizeBottomRight);

  EXPECT_EQ(window->hitTest(Point{ 150.0f, 15.0f }), WindowHit::Close);
  EXPECT_EQ(window->hitTest(Point{ 100.0f, 15.0f }), WindowHit::Maximize);
  EXPECT_EQ(window->hitTest(Point{ 50.0f, 15.0f }), WindowHit::Minimize);

  EXPECT_EQ(window->hitTest(Point{ -1.0f, 10.0f }), WindowHit::None);
  EXPECT_EQ(window->hitTest(Point{ 300.0f, 10.0f }), WindowHit::None);
}

TEST(UiWindow, SystemDecorationsLeaveTheWholeClientAreaToTheApp) {
  auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  EXPECT_EQ(window->decorations(), Window::Decorations::System);
  EXPECT_EQ(window->hitTest(Point{ 5.0f, 5.0f }), WindowHit::Client)
    << "the OS owns the frame, so every inside point is client area";
  window->setDecorations(Window::Decorations::Client);
  WindowChromeMetrics metrics;
  metrics.titleBarHeight = 30.0f;
  metrics.resizeBorder = 5.0f;
  metrics.buttonWidth = 40.0f;
  metrics.maximizable = false;
  window->setChromeMetrics(metrics);
  // With maximize disabled the band shrinks: close is still last, minimize sits before it and the
  // freed button slot becomes caption area.
  EXPECT_EQ(window->hitTest(Point{ 150.0f, 15.0f }), WindowHit::Close);
  EXPECT_EQ(window->hitTest(Point{ 100.0f, 15.0f }), WindowHit::Minimize);
  EXPECT_EQ(window->hitTest(Point{ 70.0f, 15.0f }), WindowHit::Caption);
}

TEST(UiWindow, ManagerFindsTheWindowUnderAScreenPoint) {
  WindowManager manager;
  auto first = shellWindow();
  auto second = shellWindow();
  if (first == nullptr || second == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  manager.add(first);
  manager.add(second);

  // The headless platform reports the same surface rectangle for both windows, so this checks the
  // stacking rule (the later window wins) and the miss; distinct screen rectangles need native
  // window bounds.
  EXPECT_EQ(manager.windowAt({ 10.0f, 10.0f }), second.get());
  EXPECT_EQ(manager.windowAt({ 500.0f, 500.0f }), nullptr) << "outside every window";
}

namespace {

class DropSinkLayer final : public Layer {
 public:
  DragEffect onDragEnter(const DragEvent& event) override {
    effect = event.effect;
    return DragEffect::Copy;
  }
  DragEffect onDragOver(const DragEvent& event) override {
    effect = event.effect;
    return DragEffect::Copy;
  }
  bool onDrop(const DropEvent& event) override {
    accepted = true;
    text = event.payload.data();
    return true;
  }

  DragEffect effect = DragEffect::None;
  bool accepted = false;
  std::string text;
};

} // namespace

TEST(UiWindow, ManagerRoutesADragAcrossWindows) {
  auto first = shellWindow();
  auto second = shellWindow();
  if (first == nullptr || second == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto sink = std::make_shared<DropSinkLayer>();
  sink->setBounds(Rect::MakeWH(160.0f, 90.0f));
  second->setRoot(sink);

  WindowManager manager;
  manager.add(first);
  manager.add(second);
  ASSERT_TRUE(manager.beginDrag(first, DragPayload::text("payload")));
  EXPECT_TRUE(manager.dragging());
  ASSERT_TRUE(manager.dragTo({ 20.0f, 20.0f }));
  EXPECT_EQ(manager.dragTarget(), second.get()) << "the drag routes to the window under the point";
  ASSERT_TRUE(manager.drop());
  EXPECT_TRUE(sink->accepted);
  EXPECT_EQ(sink->text, "payload");
  EXPECT_FALSE(manager.dragging()) << "the drop ends the session";
}

TEST(UiWindow, ManagerCanCancelADragWithoutDropping) {
  auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto sink = std::make_shared<DropSinkLayer>();
  sink->setBounds(Rect::MakeWH(160.0f, 90.0f));
  window->setRoot(sink);

  WindowManager manager;
  manager.add(window);
  ASSERT_TRUE(manager.beginDrag(window, DragPayload::text("cancel me")));
  ASSERT_TRUE(manager.dragTo({ 20.0f, 20.0f }));
  manager.cancelDrag();
  EXPECT_FALSE(manager.dragging());
  EXPECT_FALSE(sink->accepted) << "a cancelled drag never drops";
}
TEST(UiWindow, HeadlessWindowCannotStartAPlatformDrag) {
  auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  EXPECT_EQ(window->beginPlatformDrag(DragPayload::text("x"), DragEffect::Copy), DragEffect::None)
    << "there is no native drag source behind a headless window";
}

TEST(UiTheme, AHighContrastPaletteMapsTheSystemRolesAndDropsTheShadows) {
  SystemColors colors;
  colors.valid = true;
  colors.window = 0xFF101010u;
  colors.windowText = 0xFFF0F0F0u;
  colors.buttonFace = 0xFF202020u;
  colors.buttonText = 0xFFE0E0E0u;
  colors.highlight = 0xFF00A0FFu;
  colors.highlightText = 0xFF000000u;
  colors.grayText = 0xFF808080u;
  ASSERT_TRUE(isUsable(colors));

  const Theme theme = highContrastTheme(colors);
  EXPECT_EQ(theme.background, Color::MakeRGBA8(0x10, 0x10, 0x10, 255));
  EXPECT_EQ(theme.surface, theme.background);
  EXPECT_EQ(theme.text, Color::MakeRGBA8(0xF0, 0xF0, 0xF0, 255));
  EXPECT_EQ(theme.textMuted, Color::MakeRGBA8(0x80, 0x80, 0x80, 255));
  EXPECT_EQ(theme.accent, Color::MakeRGBA8(0x00, 0xA0, 0xFF, 255));
  EXPECT_EQ(theme.controls.background, Color::MakeRGBA8(0x20, 0x20, 0x20, 255));
  EXPECT_EQ(theme.controls.foreground, Color::MakeRGBA8(0xE0, 0xE0, 0xE0, 255));
  EXPECT_EQ(theme.controls.borderFocused, theme.accent);
  EXPECT_EQ(theme.controls.disabled, theme.textMuted);
  EXPECT_GE(theme.focusRingWidth, 2.0f) << "high contrast needs a visible focus ring";
  EXPECT_EQ(theme.shadow.sm.color.alpha, 0.0f) << "decorative elevation is disabled in this mode";
  EXPECT_EQ(theme.shadow.lg.color.alpha, 0.0f);

  SystemColors incomplete = colors;
  incomplete.highlight = 0;
  EXPECT_FALSE(isUsable(incomplete)) << "a palette missing a role keeps the application theme";
  SystemColors invalid = colors;
  invalid.valid = false;
  EXPECT_FALSE(isUsable(invalid));
}

TEST(UiWindow, AHighContrastAppearanceReplacesAndThenRestoresTheTheme) {
  const auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  window->setThemeMode(Window::ThemeMode::Dark);
  const Color darkBackground = window->theme().background;
  EXPECT_FALSE(window->highContrast());

  SystemAppearance appearance;
  appearance.valid = true;
  appearance.highContrast = true;
  appearance.reduceMotion = true;
  appearance.colors.valid = true;
  appearance.colors.window = 0xFF101010u;
  appearance.colors.windowText = 0xFFF0F0F0u;
  appearance.colors.buttonFace = 0xFF202020u;
  appearance.colors.buttonText = 0xFFE0E0E0u;
  appearance.colors.highlight = 0xFF00A0FFu;
  appearance.colors.highlightText = 0xFF000000u;
  appearance.colors.grayText = 0xFF808080u;
  window->applySystemAppearance(appearance);

  EXPECT_TRUE(window->highContrast());
  EXPECT_EQ(window->theme().background, Color::MakeRGBA8(0x10, 0x10, 0x10, 255));
  EXPECT_TRUE(reduceMotion()) << "the system's animation setting applies process-wide";

  // An app theme installed while high contrast is on is remembered, not applied ...
  Theme custom = Theme::dark();
  custom.background = Color::MakeRGBA8(1, 2, 3, 255);
  window->setTheme(custom);
  EXPECT_EQ(window->theme().background, Color::MakeRGBA8(0x10, 0x10, 0x10, 255));

  // ... and turning the mode off gives it back.
  appearance.highContrast = false;
  appearance.reduceMotion = false;
  window->applySystemAppearance(appearance);
  EXPECT_FALSE(window->highContrast());
  EXPECT_EQ(window->theme().background, Color::MakeRGBA8(1, 2, 3, 255))
    << "the theme installed during high contrast is what comes back";
  EXPECT_FALSE(reduceMotion());
}
TEST(UiWindow, LocaleKnowsWhichLanguagesAreRightToLeft) {
  for (const char* tag: { "ar", "ar-EG", "he", "fa", "ur", "ps", "sd", "ug", "yi", "dv", "ckb", "ku" }) {
    EXPECT_TRUE(Locale::parse(tag).rtl()) << tag;
  }
  for (const char* tag: { "en", "en-US", "zh-CN", "ja", "de", "ru", "hi", "tr" }) {
    EXPECT_FALSE(Locale::parse(tag).rtl()) << tag;
  }
}

TEST(UiWindow, LocaleSystemReportsTheOperatingSystemsLocale) {
  const Locale system = Locale::system();
  // A machine with no locale reports English, which is what the catalogs fall back to.
  EXPECT_FALSE(system.language.empty());
  EXPECT_EQ(Locale::parse(system.tag()).tag(), system.tag())
    << "the tag the system reports has to parse back to itself";
}
TEST(UiCrashReporter, ListsPurgesAndBundlesTheReportsADirectoryHolds) {
  const std::filesystem::path directory = std::filesystem::current_path() /
    ("coral-support-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  ASSERT_TRUE(std::filesystem::create_directories(directory, error)) << error.message();
  struct RemoveDirectory {
    std::filesystem::path path;
    ~RemoveDirectory() {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  } const guard{ directory };

  const auto write = [&directory](const std::string& name, const std::string& bytes) {
    std::ofstream file(directory / name, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  };
  write("crash-1-100.dmp", "MDMP");
  write("crash-1-100.txt", "coral ui crash report\n");
  write("notes.txt", "not a crash report\n");

  const std::vector<CrashReportFile> reports = listCrashReports(directory.string());
  ASSERT_EQ(reports.size(), 2u);
  bool sawDump = false;
  for (const CrashReportFile& report: reports) {
    if (report.miniDump) {
      sawDump = true;
      EXPECT_EQ(report.bytes, 4u);
      EXPECT_NE(report.path.find("crash-1-100.dmp"), std::string::npos);
    }
  }
  EXPECT_TRUE(sawDump) << "the minidump is reported as one";

  SupportBundle bundle;
  bundle.directory = directory.string();
  bundle.outputPath = (directory / "coral-support.txt").string();
  bundle.application = "Coral Editor";
  bundle.version = "0.1.0";
  bundle.commit = "5bf869874";
  bundle.details = { "gpu: raster", "window: 320x200" };
  ASSERT_TRUE(writeSupportBundle(bundle));
  std::ifstream bundleFile(bundle.outputPath);
  ASSERT_TRUE(bundleFile.good());
  const std::string text((std::istreambuf_iterator<char>(bundleFile)), std::istreambuf_iterator<char>());
  EXPECT_NE(text.find("application Coral Editor"), std::string::npos) << text;
  EXPECT_NE(text.find("version 0.1.0"), std::string::npos) << text;
  EXPECT_NE(text.find("commit 5bf869874"), std::string::npos) << text;
  EXPECT_NE(text.find("crash reports 2"), std::string::npos) << text;
  EXPECT_NE(text.find("crash-1-100.dmp"), std::string::npos) << text;
  EXPECT_NE(text.find("gpu: raster"), std::string::npos) << text;
  EXPECT_EQ(text.find("notes.txt"), std::string::npos) << "only crash reports are listed";

  EXPECT_TRUE(purgeCrashReports(directory.string()));
  EXPECT_TRUE(listCrashReports(directory.string()).empty());
  EXPECT_TRUE(std::filesystem::exists(directory / "notes.txt"))
    << "purging reports must not delete unrelated files";
  EXPECT_TRUE(std::filesystem::exists(bundle.outputPath)) << "the bundle is not a crash report";
}
namespace {

/** Records the pointer events that reach it, fields included. */
class PointerRecordingLayer final : public Layer {
 public:
  bool onPointerDown(const PointerEvent& event) override {
    lastDown = event;
    ++downs;
    return true;
  }
  bool onPointerMove(const PointerEvent& event) override {
    lastMove = event;
    ++moves;
    return true;
  }
  bool onPointerUp(const PointerEvent& event) override {
    lastUp = event;
    ++ups;
    return true;
  }
  PointerEvent lastDown;
  PointerEvent lastMove;
  PointerEvent lastUp;
  int downs = 0;
  int moves = 0;
  int ups = 0;
};

} // namespace

TEST(UiWindow, APointerEventCarriesItsDeviceIdentityToTheLayer) {
  const auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto sink = std::make_shared<PointerRecordingLayer>();
  sink->setBounds(Rect::MakeWH(160.0f, 90.0f));
  window->setRoot(sink);
  ASSERT_TRUE(window->renderFrame());

  PointerEvent down;
  down.position = { 20.0f, 20.0f };
  down.kind = PointerKind::Touch;
  down.pointerId = 7;
  down.pressure = 0.5f;
  ASSERT_TRUE(window->pointerDown(down));
  ASSERT_EQ(sink->downs, 1);
  EXPECT_EQ(sink->lastDown.kind, PointerKind::Touch);
  EXPECT_EQ(sink->lastDown.pointerId, 7u);
  EXPECT_FLOAT_EQ(sink->lastDown.pressure, 0.5f);

  PointerEvent up;
  up.position = { 20.0f, 20.0f };
  up.kind = PointerKind::Pen;
  up.pointerId = 9;
  up.pressure = 0.0f;
  ASSERT_TRUE(window->pointerUp(up));
  ASSERT_EQ(sink->ups, 1);
  EXPECT_EQ(sink->lastUp.kind, PointerKind::Pen);
  EXPECT_EQ(sink->lastUp.pointerId, 9u);
  EXPECT_FLOAT_EQ(sink->lastUp.pressure, 0.0f);
}

TEST(UiWindow, APointerCaptureKeepsReceivingMovesOutsideItsBounds) {
  const auto window = shellWindow();
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto root = std::make_shared<Layer>();
  root->setBounds(Rect::MakeWH(160.0f, 90.0f));
  auto sink = std::make_shared<PointerRecordingLayer>();
  sink->setBounds(Rect::MakeWH(32.0f, 32.0f));
  root->addChild(sink);
  window->setRoot(root);
  ASSERT_TRUE(window->renderFrame());

  PointerEvent down;
  down.position = { 10.0f, 10.0f };
  ASSERT_TRUE(window->pointerDown(down));
  ASSERT_EQ(sink->downs, 1);

  window->setPointerCapture(sink);
  EXPECT_EQ(window->pointerCapture(), sink.get());

  // Outside the sink's 32x32 bounds: without a capture the hit test would miss it.
  PointerEvent move;
  move.position = { 140.0f, 80.0f };
  ASSERT_TRUE(window->pointerMove(move));
  ASSERT_EQ(sink->moves, 1);
  EXPECT_FLOAT_EQ(sink->lastMove.position.x, 140.0f);
  EXPECT_FLOAT_EQ(sink->lastMove.position.y, 80.0f);

  PointerEvent up;
  up.position = { 140.0f, 80.0f };
  ASSERT_TRUE(window->pointerUp(up));
  ASSERT_EQ(sink->ups, 1);
  EXPECT_EQ(window->pointerCapture(), nullptr) << "a pointer up releases the capture";
}

TEST(UiWindow, OverlayHostMountsAndDismissesAnElement) {
  Window::Options options;
  options.width = 320;
  options.height = 200;
  options.headless = true;
  options.frameLimit = 0;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";

  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 24, 30, 255)));

  OverlayHost host(*window);
  ElementProps props;
  props.bounds = Rect::MakeWH(120.0f, 48.0f);
  const uint64_t id = host.show(components::WithProps(
    std::make_shared<ColorElement>(Color::MakeRGBA8(200, 100, 50, 255)), props), {});
  ASSERT_NE(id, 0u);
  EXPECT_TRUE(window->hasOverlay());
  EXPECT_EQ(host.size(), 1u);
  EXPECT_TRUE(host.contains(id));

  EXPECT_TRUE(host.dismiss(id));
  EXPECT_FALSE(window->hasOverlay());
  EXPECT_EQ(host.size(), 0u);
}

TEST(UiWindow, DslOverlayBuildsAndClosesTransientSurfaces) {
  Window::Options options;
  options.width = 320;
  options.height = 200;
  options.headless = true;
  options.frameLimit = 0;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 24, 30, 255)));

  dsl::Overlay overlays(*window, Theme::dark());
  const uint64_t toast = overlays.Toast("saved", std::chrono::milliseconds(10));
  ASSERT_NE(toast, 0u);
  EXPECT_TRUE(window->hasOverlay());
  EXPECT_EQ(overlays.openCount(), 1u);
  EXPECT_TRUE(overlays.close(toast));
  EXPECT_FALSE(window->hasOverlay());

  const uint64_t dialog = overlays.Dialog("Delete build", "This cannot be undone", {}, {
    dsl::DialogButton{ "Cancel", {} },
    dsl::DialogButton{ "Delete", {} },
  });
  ASSERT_NE(dialog, 0u);
  EXPECT_TRUE(window->hasOverlay());
  EXPECT_EQ(overlays.openCount(), 1u);
  overlays.closeAll();
  EXPECT_FALSE(window->hasOverlay());
  EXPECT_EQ(overlays.openCount(), 0u);

  const uint64_t tooltip = overlays.Tooltip("hint", Point{ 12.0f, 14.0f });
  ASSERT_NE(tooltip, 0u);
  EXPECT_TRUE(overlays.close(tooltip));

  OverlayOptions anchored;
  anchored.anchor = Rect::MakeXYWH(20.0f, 20.0f, 4.0f, 4.0f);
  const uint64_t popup = overlays.Popup(
    std::make_shared<ColorLayer>(Color::MakeRGBA8(80, 90, 100, 255)), anchored);
  ASSERT_NE(popup, 0u);
  EXPECT_TRUE(overlays.close(popup));

  const uint64_t menu = overlays.Menu({ "One", "Two" }, Point{ 30.0f, 30.0f }, {});
  ASSERT_NE(menu, 0u);
  EXPECT_TRUE(overlays.close(menu));
  EXPECT_FALSE(window->hasOverlay());
}

TEST(UiWindow, ToastDismissRemovesTheOverlayEntry) {
  Window::Options options;
  options.width = 320;
  options.height = 200;
  options.headless = true;
  options.frameLimit = 0;
  auto window = Window::create(options);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  auto page = std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 24, 30, 255));
  window->setRoot(page);

  OverlayHost host(*window);
  auto toast = std::make_shared<Toast>("saved", nullptr);
  toast->showFor("saved", std::chrono::milliseconds(1));
  OverlayOptions overlayOptions;
  overlayOptions.anchor = Rect::MakeXYWH(0.0f, 150.0f, 320.0f, 32.0f);
  overlayOptions.preferred = Placement::Center;
  overlayOptions.passThroughPointer = true;
  const uint64_t id = host.show(dsx::Layer(toast, "toast-test"), overlayOptions);
  ASSERT_NE(id, 0u);
  EXPECT_EQ(window->root()->hitTest(Point{ 10.0f, 10.0f }), page.get());
  toast->setOnDismissed([&host, id] { host.dismiss(id); });
  EXPECT_TRUE(toast->visible());
  EXPECT_TRUE(toast->hasRunningAnimation());
  EXPECT_TRUE(window->root()->hasRunningAnimation());
  window->requestFrame();
  window->renderFrame();
  EXPECT_LT(toast->bounds().getHeight(), 100.0f);
  for (int frame = 0; frame < 20 && window->hasOverlay(); ++frame) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    window->requestFrame();
    window->renderFrame();
  }
  EXPECT_FALSE(window->hasOverlay());
  EXPECT_EQ(host.size(), 0u);
}

TEST(UiWindow, OverlaySurvivesAPageRebuild) {
  Window::Options windowOptions;
  windowOptions.width = 320;
  windowOptions.height = 200;
  windowOptions.headless = true;
  windowOptions.frameLimit = 0;
  auto window = Window::create(windowOptions);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";

  Reconciler reconciler;
  window->setRoot(reconciler.mount(dsl::Column(dsl::StackOptions{ .key = "page" },
    dsl::Text("first"))));

  OverlayHost host(*window);
  auto toast = std::make_shared<Toast>("saved", nullptr);
  toast->showFor("saved", std::chrono::seconds(10));
  OverlayOptions overlayOptions;
  overlayOptions.anchor = Rect::MakeXYWH(0.0f, 150.0f, 320.0f, 32.0f);
  overlayOptions.passThroughPointer = true;
  const uint64_t id = host.show(dsx::Layer(toast, "toast-rebuild"), overlayOptions);
  ASSERT_NE(id, 0u);

  reconciler.update(dsl::Column(dsl::StackOptions{ .key = "page" },
    dsl::Text("second")));
  window->requestFrame();
  window->renderFrame();

  EXPECT_TRUE(window->hasOverlay());
  EXPECT_NE(toast->parent(), nullptr);
  EXPECT_TRUE(toast->visible());
}

TEST(UiWindow, OverlayDoesNotTakePageLayoutSpace) {
  Window::Options windowOptions;
  windowOptions.width = 320;
  windowOptions.height = 200;
  windowOptions.headless = true;
  windowOptions.frameLimit = 0;
  auto window = Window::create(windowOptions);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";

  Reconciler reconciler;
  auto root = reconciler.mount(dsl::Row(dsl::StackOptions{ .key = "root" },
    dsl::Swatch(Color::MakeRGBA8(200, 80, 40, 255), 100.0f, 50.0f, 0.0f, "left"),
    dsl::Swatch(Color::MakeRGBA8(40, 120, 200, 255), 100.0f, 50.0f, 0.0f, "right")));
  window->setRoot(root);

  OverlayHost host(*window);
  auto toast = std::make_shared<Toast>("saved", nullptr);
  toast->showFor("saved", std::chrono::seconds(10));
  OverlayOptions overlayOptions;
  overlayOptions.anchor = Rect::MakeXYWH(0.0f, 150.0f, 320.0f, 32.0f);
  overlayOptions.passThroughPointer = true;
  ASSERT_NE(host.show(dsx::Layer(toast, "toast-layout"), overlayOptions), 0u);

  window->requestFrame();
  window->renderFrame();

  const auto flex = std::dynamic_pointer_cast<Flex>(root);
  ASSERT_NE(flex, nullptr);
  ASSERT_GE(flex->children().size(), 2u);
  EXPECT_FLOAT_EQ(flex->children()[0]->bounds().getWidth(), 100.0f);
  EXPECT_FLOAT_EQ(flex->children()[1]->bounds().getWidth(), 100.0f);
}

TEST(UiWindow, TwoToastsDismissWithoutCrash) {
  Window::Options windowOptions;
  windowOptions.width = 320;
  windowOptions.height = 200;
  windowOptions.headless = true;
  windowOptions.frameLimit = 0;
  auto window = Window::create(windowOptions);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 24, 30, 255)));

  OverlayHost host(*window);
  auto first = std::make_shared<Toast>("one", nullptr);
  first->showFor("one", std::chrono::milliseconds(1));
  const uint64_t firstId = host.show(dsx::Layer(first, "toast-one"));
  first->setOnDismissed([&host, firstId] { host.dismiss(firstId); });
  auto second = std::make_shared<Toast>("two", nullptr);
  second->showFor("two", std::chrono::milliseconds(1));
  const uint64_t secondId = host.show(dsx::Layer(second, "toast-two"));
  second->setOnDismissed([&host, secondId] { host.dismiss(secondId); });
  ASSERT_NE(firstId, 0u);
  ASSERT_NE(secondId, 0u);

  for (int frame = 0; frame < 20 && window->hasOverlay(); ++frame) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    window->requestFrame();
    window->renderFrame();
  }
  EXPECT_FALSE(window->hasOverlay());
  EXPECT_EQ(host.size(), 0u);
}

TEST(UiWindow, SplitButtonMenuIsFloatingAndDoesNotShiftThePage) {
  Window::Options windowOptions;
  windowOptions.width = 480;
  windowOptions.height = 240;
  windowOptions.headless = true;
  windowOptions.frameLimit = 0;
  auto window = Window::create(windowOptions);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";

  Reconciler reconciler;
  auto root = reconciler.mount(dsl::Column(dsl::StackOptions{ .key = "page" },
    dsx::SplitButton("Run", nullptr, { "Run all", "Debug" }, {}, {}, "run")));
  window->setRoot(root);
  window->requestFrame();
  window->renderFrame();

  const auto page = std::dynamic_pointer_cast<Flex>(root);
  ASSERT_NE(page, nullptr);
  ASSERT_FALSE(page->children().empty());
  const auto split = std::dynamic_pointer_cast<SplitButton>(page->children().front());
  ASSERT_NE(split, nullptr);
  const Rect before = split->bounds();
  const Rect button = split->boundsInWindow();

  PointerEvent press;
  press.position = Point{ button.getRight() - 4.0f, button.getTop() + button.getHeight() * 0.5f };
  press.button = PointerButton::Primary;
  window->pointerDown(press);
  window->pointerUp(press);
  window->requestFrame();
  window->renderFrame();

  ASSERT_GE(page->children().size(), 2u);
  const auto menu = std::dynamic_pointer_cast<Menu>(page->children().back());
  ASSERT_NE(menu, nullptr);
  EXPECT_TRUE(menu->floating());
  EXPECT_TRUE(menu->visible());
  EXPECT_FLOAT_EQ(split->bounds().getLeft(), before.getLeft());
  EXPECT_FLOAT_EQ(split->bounds().getWidth(), before.getWidth());

}

TEST(UiWindow, DslOverlayAcceptsElementContent) {
  Window::Options windowOptions;
  windowOptions.width = 360;
  windowOptions.height = 240;
  windowOptions.headless = true;
  windowOptions.frameLimit = 0;
  auto window = Window::create(windowOptions);
  if (window == nullptr) GTEST_SKIP() << "no GPU device is available for the headless surface";
  window->setRoot(std::make_shared<ColorLayer>(Color::MakeRGBA8(20, 24, 30, 255)));

  dsl::Overlay overlays(*window, Theme::dark());
  OverlayOptions anchored;
  anchored.anchor = Rect::MakeXYWH(20.0f, 100.0f, 160.0f, 24.0f);
  const uint64_t popup = overlays.PopupElement(dsl::Text("element popup"), anchored);
  ASSERT_NE(popup, 0u);
  EXPECT_TRUE(window->hasOverlay());
  EXPECT_TRUE(overlays.close(popup));

  const uint64_t dialog = overlays.DialogElement("Element dialog", "body", dsl::Text("content"), {
    dsl::DialogButton{ "OK", {} },
  });
  ASSERT_NE(dialog, 0u);
  EXPECT_TRUE(window->hasOverlay());
  EXPECT_TRUE(overlays.close(dialog));
}