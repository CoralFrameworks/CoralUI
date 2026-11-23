/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// A deliberately small native-window smoke test.
//
// `test_ui` already contains native-window cases, but they call `GTEST_SKIP` when the session cannot
// create a window, which makes a platform regression indistinguishable from "the runner has no
// desktop". This executable returns 77 in that case so CTest reports a skip, and fails loudly when a
// window *was* created but the first frame, resize or DPI report is wrong.

#include <ui/Button.h>
#include <ui/ColorLayer.h>
#include <ui/Window.h>

#include <graphics/Bitmap.h>
#include <graphics/Color.h>

#include <cstdio>
#include <cstdlib>
#include <memory>

int main() {
#if defined(__APPLE__)
  (void)setenv("CORAL_UI_FRAME_PLAN_WINDOW", "1", 1);
#endif
  coral::ui::Window::Options options;
  options.headless = false;
  options.width = 320;
  options.height = 200;
  options.frameLimit = 1;
  options.clearColor = graphics::Color::MakeRGBA8(24, 26, 32, 255);

  auto window = coral::ui::Window::create(options);
  if (window == nullptr) {
    std::fprintf(stderr, "SKIP: no native window could be created in this session\n");
    return 77;
  }

  if (window->width() <= 0 || window->height() <= 0) {
    std::fprintf(stderr, "FAIL: native window reported an empty client area\n");
    return 1;
  }
  if (!(window->uiScale() > 0.0f)) {
    std::fprintf(stderr, "FAIL: native window reported uiScale=%g\n",
      static_cast<double>(window->uiScale()));
    return 1;
  }
  if (window->devicePixelRatio() != window->uiScale()) {
    std::fprintf(stderr, "FAIL: devicePixelRatio and uiScale disagree\n");
    return 1;
  }

  // Window-manager and appearance paths that a native session is the only way to exercise: the
  // title reaches the compositor, the cursor shape reaches the seat, and the system theme is read
  // from the desktop rather than defaulted.
  window->setTitle("Coral native smoke");
  window->setCursor(coral::ui::Window::Cursor::IBeam);
  window->setAlwaysOnTop(false);
  window->setOpacity(0.75f);
  window->maximize();
  if (!window->isMaximized()) {
    std::fprintf(stderr, "FAIL: maximize did not update the window state\n");
    return 1;
  }
  window->restore();
  if (window->isMaximized()) {
    std::fprintf(stderr, "FAIL: restore did not clear the maximized state\n");
    return 1;
  }
  const coral::ui::SystemAppearance appearance = window->systemAppearance();
  const std::string parentHandle = window->portalParentHandle();

  auto root = std::make_shared<coral::ui::ColorLayer>(graphics::Color::MakeRGBA8(56, 132, 220, 255));
  auto focusable = std::make_shared<coral::ui::Button>("Focus", nullptr);
  focusable->setBounds(coral::ui::Rect::MakeLTRB(8.0f, 8.0f, 80.0f, 40.0f));
  focusable->setRepaintBoundary(true);
  root->addChild(focusable);
  window->setRoot(root);
  window->focusLayer(focusable.get());
  if (!window->renderFrame()) {
    std::fprintf(stderr, "FAIL: the first native frame did not render: %s\n",
      window->diagnostics().c_str());
    return 1;
  }
  if (window->frameCount() < 1) {
    std::fprintf(stderr, "FAIL: the native frame loop did not count the first frame\n");
    return 1;
  }

  // A clean native window replays the retained recording instead of re-recording the whole tree.
  // Only the Cocoa backend currently opts into retained frames; the other native backends have
  // their own suites for this.
  if (window->nativePlatform() == coral::ui::Window::NativePlatform::Cocoa) {
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: the retained native frame did not render\n");
      return 1;
    }
    if (window->lastFrameTimings().draw != 0.0) {
      std::fprintf(stderr, "FAIL: a clean native frame recorded %.2fms instead of replaying\n",
        window->lastFrameTimings().draw);
      return 1;
    }
    if (!(window->refreshHertz() > 0.0)) {
      std::fprintf(stderr, "FAIL: CVDisplayLink did not report a refresh rate\n");
      return 1;
    }
  }

  // Damage: repaint a small child and let the damage path present it; the snapshot must reflect the
  // new colour while the untouched area keeps the root colour.
  auto damaged = std::make_shared<coral::ui::ColorLayer>(graphics::Color::MakeRGBA8(255, 0, 0, 255));
  damaged->setBounds(coral::ui::Rect::MakeLTRB(0.0f, 0.0f, 40.0f, 40.0f));
  damaged->setRepaintBoundary(true);
  root->addChild(damaged);
  if (!window->renderFrame()) {
    std::fprintf(stderr, "FAIL: the native frame with a damage child did not render\n");
    return 1;
  }
  damaged->setColor(graphics::Color::MakeRGBA8(0, 255, 0, 255));
  window->invalidate(coral::ui::Rect::MakeLTRB(0.0f, 0.0f, 40.0f, 40.0f));
  if (!window->renderFrame()) {
    std::fprintf(stderr, "FAIL: the damaged native frame did not render\n");
    return 1;
  }
  graphics::Bitmap bitmap;
  if (!window->snapshot(&bitmap)) {
    std::fprintf(stderr, "FAIL: the damaged native frame could not be snapshotted\n");
    return 1;
  }
  const graphics::Pixmap pixels = bitmap.makePixmap();
  if (!pixels.isValid() || (pixels.getPixelARGB32(10, 10) & 0x00FFFFFFu) != 0x0000FF00u ||
      (pixels.getPixelARGB32(200, 100) & 0x00FFFFFFu) != 0x003884DCu) {
    std::fprintf(stderr, "FAIL: damaged snapshot colours were 0x%08x / 0x%08x\n",
      pixels.isValid() ? pixels.getPixelARGB32(10, 10) : 0u,
      pixels.isValid() ? pixels.getPixelARGB32(200, 100) : 0u);
    return 1;
  }

  if (!window->resize(360, 240)) {
    std::fprintf(stderr, "FAIL: the native window refused a resize\n");
    return 1;
  }
  if (!window->renderFrame()) {
    std::fprintf(stderr, "FAIL: the native frame after resize did not render: %s\n",
      window->diagnostics().c_str());
    return 1;
  }

  std::printf("native ui smoke: %dx%d uiScale=%.2f frames=%d appearance=%s dark=%d highContrast=%d "
              "parent=%s\n", window->width(), window->height(),
    static_cast<double>(window->uiScale()), window->frameCount(),
    appearance.valid ? "valid" : "unavailable", appearance.preferDark ? 1 : 0,
    appearance.highContrast ? 1 : 0, parentHandle.empty() ? "none" : parentHandle.c_str());
  if (window->nativePlatform() == coral::ui::Window::NativePlatform::Cocoa) {
    const coral::ui::FrameStatistics stats = window->frameStatistics();
    std::printf("native refresh: %.1f Hz, plan items=%u, buffers=%u\n",
      static_cast<double>(window->refreshHertz()), stats.framePlanItems, stats.commandBuffers);
    if (stats.framePlanItems == 0 || stats.commandBuffers != 1) {
      std::fprintf(stderr, "FAIL: Cocoa plan frame was flattened (items=%u buffers=%u)\n",
        stats.framePlanItems, stats.commandBuffers);
      return 1;
    }
  }
  return 0;
}
