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

#include <ui/WindowRestoration.h>
#include <memory>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <ui/win32/NativeButton.h>
#include <ui/win32/NativeDatePicker.h>
#include <ui/win32/NativePopUpButton.h>
#include <ui/win32/NativeProgressIndicator.h>
#include <ui/win32/NativeSearchField.h>
#include <ui/win32/NativeSlider.h>
#include <ui/win32/NativeStepper.h>
#include <ui/win32/NativeSwitch.h>

#include <chrono>
#include <string>
#endif

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
#if defined(_WIN32)
  int nativeButtonClicks = 0;
  std::string nativeSearchText;
  int nativeSwitchChanges = 0;
  bool nativeSwitchOn = false;
  auto nativeButton = coral::ui::win32::NativeButton::create("Native",
    [&] { ++nativeButtonClicks; });
  auto nativeSwitch = coral::ui::win32::NativeSwitch::create("On", true,
    [&](const bool on) { ++nativeSwitchChanges; nativeSwitchOn = on; });
  auto nativeProgress = coral::ui::win32::NativeProgressIndicator::create(false);
  auto nativeSearch = coral::ui::win32::NativeSearchField::create("Search",
    [&](std::string_view text) { nativeSearchText = text; });
  auto nativePopup = coral::ui::win32::NativePopUpButton::create({ "Alpha", "Beta" }, 1);
  auto nativeSlider = coral::ui::win32::NativeSlider::create(0.0, 100.0, 35.0);
  auto nativeStepper = coral::ui::win32::NativeStepper::create(0.0, 10.0, 4.0);
  auto nativeDate = coral::ui::win32::NativeDatePicker::create(
    std::chrono::year_month_day{ std::chrono::year{ 2026 }, std::chrono::month{ 11 },
      std::chrono::day{ 26 } });
  if (!nativeButton || !nativeSwitch || !nativeProgress || !nativeSearch || !nativePopup ||
      !nativeSlider || !nativeStepper || !nativeDate) {
    std::fprintf(stderr, "FAIL: a Win32 native control wrapper could not be created\n");
    return 1;
  }
  nativeButton->setBounds(coral::ui::Rect::MakeXYWH(120.0f, 8.0f, 100.0f, 28.0f));
  nativeSwitch->setBounds(coral::ui::Rect::MakeXYWH(230.0f, 8.0f, 80.0f, 28.0f));
  nativeProgress->setBounds(coral::ui::Rect::MakeXYWH(8.0f, 44.0f, 160.0f, 20.0f));
  nativeProgress->setValue(0.65);
  nativeSearch->setBounds(coral::ui::Rect::MakeXYWH(8.0f, 72.0f, 180.0f, 28.0f));
  nativePopup->setBounds(coral::ui::Rect::MakeXYWH(200.0f, 72.0f, 110.0f, 28.0f));
  nativeSlider->setBounds(coral::ui::Rect::MakeXYWH(8.0f, 108.0f, 160.0f, 28.0f));
  nativeSlider->setValue(42.0);
  nativeStepper->setBounds(coral::ui::Rect::MakeXYWH(176.0f, 108.0f, 50.0f, 28.0f));
  nativeStepper->setValue(7.0);
  nativeDate->setBounds(coral::ui::Rect::MakeXYWH(234.0f, 108.0f, 80.0f, 28.0f));
  for (const auto& native: { std::static_pointer_cast<coral::ui::Layer>(nativeButton),
      std::static_pointer_cast<coral::ui::Layer>(nativeSwitch),
      std::static_pointer_cast<coral::ui::Layer>(nativeProgress),
      std::static_pointer_cast<coral::ui::Layer>(nativeSearch),
      std::static_pointer_cast<coral::ui::Layer>(nativePopup),
      std::static_pointer_cast<coral::ui::Layer>(nativeSlider),
      std::static_pointer_cast<coral::ui::Layer>(nativeStepper),
      std::static_pointer_cast<coral::ui::Layer>(nativeDate) }) {
    root->addChild(native);
  }
#endif
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

#if defined(_WIN32)
  const HWND nativeHost = static_cast<HWND>(window->nativeWindowHandle());
  const auto childOf = [](const auto& control) {
    return static_cast<HWND>(control->nativeView());
  };
  const HWND nativeButtonHandle = childOf(nativeButton);
  const HWND nativeSwitchHandle = childOf(nativeSwitch);
  const HWND nativeProgressHandle = childOf(nativeProgress);
  const HWND nativeSearchHandle = childOf(nativeSearch);
  const HWND nativeSliderHandle = childOf(nativeSlider);
  const HWND nativeStepperHandle = childOf(nativeStepper);
  for (const HWND child: { nativeButtonHandle, nativeSwitchHandle, nativeProgressHandle,
      nativeSearchHandle, childOf(nativePopup), nativeSliderHandle, nativeStepperHandle,
      childOf(nativeDate) }) {
    if (child == nullptr || !::IsWindow(child) || ::GetParent(child) != nativeHost ||
        !::IsWindowVisible(child)) {
      std::fprintf(stderr, "FAIL: a Win32 native control was not attached to the window\n");
      return 1;
    }
  }

  ::SendMessageW(nativeButtonHandle, BM_CLICK, 0, 0);
  if (nativeButtonClicks != 1) {
    std::fprintf(stderr, "FAIL: the native button did not deliver BN_CLICKED\n");
    return 1;
  }
  ::SendMessageW(nativeSwitchHandle, BM_CLICK, 0, 0);
  if (nativeSwitchChanges != 1 || nativeSwitchOn) {
    std::fprintf(stderr, "FAIL: the native switch did not deliver its toggle\n");
    return 1;
  }
  if (::SendMessageW(nativeProgressHandle, PBM_GETPOS, 0, 0) != 65) {
    std::fprintf(stderr, "FAIL: the native progress bar did not keep its value\n");
    return 1;
  }
  if (::SendMessageW(nativeSliderHandle, TBM_GETPOS, 0, 0) != 42) {
    std::fprintf(stderr, "FAIL: the native slider did not keep its value\n");
    return 1;
  }
  if (::SendMessageW(nativeStepperHandle, UDM_GETPOS32, 0, 0) != 7) {
    std::fprintf(stderr, "FAIL: the native stepper did not keep its value\n");
    return 1;
  }
  if (nativePopup->selectedIndex() != 1 || nativePopup->itemTitle(1) != "Beta") {
    std::fprintf(stderr, "FAIL: the native combo box did not keep its selection\n");
    return 1;
  }
  nativeSearch->setText("coral");
  if (nativeSearch->text() != "coral") {
    std::fprintf(stderr, "FAIL: the native search field did not keep its text\n");
    return 1;
  }
  ::SendMessageW(nativeHost, WM_COMMAND,
    MAKEWPARAM(::GetDlgCtrlID(nativeSearchHandle), EN_CHANGE),
    reinterpret_cast<LPARAM>(nativeSearchHandle));
  if (nativeSearchText != "coral") {
    std::fprintf(stderr, "FAIL: the native search field did not deliver EN_CHANGE\n");
    return 1;
  }
  const auto expectedDate = std::chrono::year_month_day{ std::chrono::year{ 2026 },
    std::chrono::month{ 11 }, std::chrono::day{ 26 } };
  if (nativeDate->date() != expectedDate) {
    std::fprintf(stderr, "FAIL: the native date picker did not keep its date\n");
    return 1;
  }
#endif

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

  if (window->nativePlatform() == coral::ui::Window::NativePlatform::Cocoa) {
    coral::ui::Window::Options hdrOptions;
    hdrOptions.headless = false;
    hdrOptions.width = 240;
    hdrOptions.height = 160;
    hdrOptions.outputColorSpace = coral::ui::OutputColorSpace::Hdr10Pq;
    hdrOptions.frameLimit = 1;
    auto hdrWindow = coral::ui::Window::create(hdrOptions);
    if (hdrWindow != nullptr) {
      if (!hdrWindow->renderFrame()) {
        std::fprintf(stderr, "FAIL: HDR request window did not render\n");
        return 1;
      }
      const coral::ui::FrameStatistics hdrStats = hdrWindow->frameStatistics();
      const bool hdr = hdrWindow->outputColorSpace() == coral::ui::OutputColorSpace::Hdr10Pq ||
        hdrWindow->outputColorSpace() == coral::ui::OutputColorSpace::ExtendedLinearP3;
      if (hdrStats.hdrOutput != hdr) {
        std::fprintf(stderr, "FAIL: HDR statistics disagree with the granted colour space\n");
        return 1;
      }
    }
  }

  if (window->nativePlatform() == coral::ui::Window::NativePlatform::Cocoa) {
    if (!coral::ui::supportsWindowRestoration()) {
      std::fprintf(stderr, "FAIL: window restoration is not advertised\n");
      return 1;
    }
    coral::ui::Window::Options restoreOptions;
    restoreOptions.headless = false;
    restoreOptions.width = 200;
    restoreOptions.height = 120;
    restoreOptions.restorationIdentifier = "coral.native.smoke";
    auto restoredWindow = coral::ui::Window::create(restoreOptions);
    if (restoredWindow == nullptr) {
      std::fprintf(stderr, "FAIL: restoration window could not be created\n");
      return 1;
    }
    if (restoredWindow->nativeWindowHandle() == nullptr ||
        restoredWindow->restorationIdentifier() != "coral.native.smoke") {
      std::fprintf(stderr, "FAIL: native window restoration state was not configured\n");
      return 1;
    }

    coral::ui::Window::Options fullSize;
    fullSize.headless = false;
    fullSize.width = 240;
    fullSize.height = 160;
    fullSize.fullSizeContentView = true;
    auto fullSizeWindow = coral::ui::Window::create(fullSize);
    if (fullSizeWindow != nullptr) {
      if (!fullSizeWindow->renderFrame()) {
        std::fprintf(stderr, "FAIL: full-size content window did not render\n");
        return 1;
      }
      if (!(fullSizeWindow->safeAreaInsets().top > 0.0f)) {
        std::fprintf(stderr, "FAIL: full-size content did not report a titlebar safe area\n");
        return 1;
      }
    }
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
    std::printf("native display: colorSpace=%d hdr=%d headroom=%.2f wideColor=%d\n",
      static_cast<int>(window->outputColorSpace()),
      window->frameStatistics().hdrOutput ? 1 : 0,
      static_cast<double>(window->displayHeadroom()), window->supportsWideColor() ? 1 : 0);
    if (window->displayHeadroom() < 1.0f) {
      std::fprintf(stderr, "FAIL: display headroom is below SDR\n");
      return 1;
    }
  }
  return 0;
}
