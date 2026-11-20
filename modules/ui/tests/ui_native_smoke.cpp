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

#include <graphics/Color.h>

#include <cstdio>
#include <memory>

int main() {
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
  return 0;
}
