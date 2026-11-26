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

/**
 * @file ui_demo.cpp
 *
 * The native-window acceptance app and cross-platform control gallery. It is deliberately built
 * only from the public `ui` layer primitives -- no editor code -- so it doubles as the manual smoke
 * test for the window, layout, input, accessibility, IME and present path.
 */

#include <ui/Avatar.h>
#include <ui/AvatarGroup.h>
#include <ui/Badge.h>
#include <ui/Banner.h>
#include <ui/Breadcrumb.h>
#include <ui/Button.h>
#include <ui/Card.h>
#include <ui/CheckBoxGroup.h>
#include <ui/Chip.h>
#include <ui/ComboBox.h>
#include <ui/DateTimeField.h>
#include <ui/DateField.h>
#include <ui/DateRangeField.h>
#include <ui/DropZone.h>
#include <ui/EmptyState.h>
#include <ui/Expander.h>
#include <ui/Flex.h>
#include <ui/FormField.h>
#include <ui/ProgressBar.h>
#include <ui/Pagination.h>
#include <ui/PasswordField.h>
#include <ui/ProgressRing.h>
#include <ui/PropertyRow.h>
#include <ui/RadioGroup.h>
#include <ui/RangeSlider.h>
#include <ui/Rating.h>
#include <ui/SearchField.h>
#include <ui/Kbd.h>
#include <ui/Link.h>
#include <ui/Meter.h>
#include <ui/MultiSelect.h>
#include <ui/SectionHeader.h>
#include <ui/Select.h>
#include <ui/Stat.h>
#include <ui/StatusBar.h>
#include <ui/StatusIndicator.h>
#include <ui/Spinner.h>
#include <ui/SplitButton.h>
#include <ui/Stepper.h>
#include <ui/TimeField.h>
#include <ui/TimeRangeField.h>
#include <ui/ToastStack.h>
#include <ui/Toolbar.h>
#include <ui/Toggle.h>
#include <ui/ToggleButton.h>
#include <ui/ToggleGroup.h>
#include <ui/ColorPanel.h>
#include <ui/ColorPicker.h>
#include <ui/Popup.h>
#include <ui/Command.h>
#include <ui/ColorLayer.h>
#include <ui/FileDialog.h>
#include <ui/FontPanel.h>
#include <ui/Notification.h>
#include <ui/ScreenCapture.h>
#include <ui/SecurityScopedBookmark.h>
#include <ui/TagList.h>
#include <ui/TagInput.h>
#include <ui/TextInput.h>
#include <ui/TextLayer.h>
#include <ui/Theme.h>

#if defined(__APPLE__)
#include <ui/macos/NativeButton.h>
#include <ui/macos/NativePopUpButton.h>
#include <ui/macos/NativeSearchField.h>
#include <ui/macos/NativeProgressIndicator.h>
#include <ui/macos/NativeSwitch.h>
#include <ui/macos/NativeStepper.h>
#endif
#include "gallery_sections.h"

#include <ui/Window.h>

#include <graphics/Data.h>
#include <graphics/Font.h>
#include <graphics/Bitmap.h>
#include <graphics/Image.h>
#include <graphics/Paint.h>
#include <graphics/Typeface.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace {

using namespace coral::ui;

std::string fontPath() {
#if defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string executable(size, '\0');
  if (_NSGetExecutablePath(executable.data(), &size) == 0) {
    const std::filesystem::path bundleFont =
      std::filesystem::path(executable.c_str()).parent_path().parent_path() /
      "Resources" / "fonts" / "NotoSansSC-Regular.otf";
    if (std::filesystem::exists(bundleFont)) return bundleFont.string();
  }
#endif
  return std::string(CORAL_ASSETS_DIR) + "/fonts/NotoSansSC-Regular.otf";
}

/** Keeps the fixed gallery page below the macOS titlebar/toolbar safe area. */
class GalleryPage final : public Layer {
 public:
  GalleryPage(Window* window, bool fullSize)
      : mWindow(window), mFullSize(fullSize) {}

  void layout() override {
    if (mWindow != nullptr) {
      const bool fullscreen = mWindow->isFullscreen();
      setTranslation({ 0.0f, (mFullSize || fullscreen) ? mWindow->safeAreaInsets().top : 0.0f });
    }
    Layer::layout();
  }

 private:
  Window* mWindow = nullptr;
  bool mFullSize = false;
};

std::shared_ptr<const graphics::Image> galleryIcon() {
  graphics::Bitmap bitmap = graphics::Bitmap::allocate(16, 16);
  graphics::Pixmap pixels = bitmap.lockPixels();
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const float dx = static_cast<float>(x) - 7.5f;
      const float dy = static_cast<float>(y) - 7.5f;
      if (dx * dx + dy * dy <= 7.0f * 7.0f) {
        pixels.setPixelARGB32(x, y, 0xFFFFFFFFu);
      }
    }
  }
  return std::make_shared<const graphics::Image>(graphics::Image::fromBitmap(bitmap));
}

bool writePng(const graphics::Bitmap& bitmap, const std::string& path) {
  if (bitmap.isEmpty()) return false;
  std::string error;
  const graphics::Data encoded = graphics::Image::fromBitmap(bitmap).encode(
    graphics::ImageFormat::PNG, 100, &error);
  if (encoded.size() == 0) {
    std::fprintf(stderr, "could not encode the UI frame: %s\n", error.c_str());
    return false;
  }
  std::ofstream stream(path, std::ios::binary);
  if (!stream.is_open()) return false;
  stream.write(reinterpret_cast<const char*>(encoded.bytes()),
    static_cast<std::streamsize>(encoded.size()));
  return stream.good();
}

} // namespace anonymous

const char* platformName(coral::ui::Window::NativePlatform platform);

int main(int argc, char** argv) {
  bool headless = false;
  int frames = 0;
  std::string output;
  int width = 960;
  int height = 700;
  Window::NativePlatform platform = Window::NativePlatform::Auto;
  float uiScale = 0.0f;
  bool focusInput = false;
  bool acceptance = false;
  bool fullSizeContentView = false;
  bool threadedHost = false;
  bool showToolbar = false;
  bool nativeDisabled = false;
  bool imeDemo = false;
  std::string themeName = "dark";
  std::string colorSpaceName = "srgb";
  bool bookmarkSmoke = false;
  bool bookmarkPanel = false;
  int gallerySection = 0;
  std::string galleryControl;
  float clickX = -1.0f;
  float clickY = -1.0f;
  bool dragScroll = false;
  bool clickTwice = false;
  bool dragRow = false;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    const auto next = [&]() -> std::string { return index + 1 < argc ? argv[++index] : std::string(); };
    if (argument == "--headless") {
      headless = true;
    } else if (argument == "--frames") {
      // `--frames 0` is the interactive mode: run until the window closes. Only a negative value is
      // meaningless here.
      frames = std::max(0, std::atoi(next().c_str()));
    } else if (argument == "--out") {
      output = next();
    } else if (argument == "--width") {
      width = std::max(320, std::atoi(next().c_str()));
    } else if (argument == "--height") {
      height = std::max(240, std::atoi(next().c_str()));
    } else if (argument == "--platform") {
      const std::string value = next();
      platform = value == "wayland" ? Window::NativePlatform::Wayland
        : (value == "x11" ? Window::NativePlatform::X11
        : (value == "cocoa" ? Window::NativePlatform::Cocoa : Window::NativePlatform::Auto));
    } else if (argument == "--toolbar") {
      showToolbar = true;
    } else if (argument == "--native-disabled") {
      nativeDisabled = true;
    } else if (argument == "--ime-demo") {
      imeDemo = true;
    } else if (argument == "--focus-input") {
      focusInput = true;
    } else if (argument == "--acceptance") {
      acceptance = true;
    } else if (argument == "--bookmark-smoke") {
      bookmarkSmoke = true;
    } else if (argument == "--bookmark-panel") {
      bookmarkPanel = true;
    } else if (argument == "--full-size") {
      fullSizeContentView = true;
    } else if (argument == "--render-host") {
      const std::string value = next();
      threadedHost = value == "render-thread" || value == "threaded";
    } else if (argument == "--scale") {
      uiScale = std::max(0.0f, static_cast<float>(std::atof(next().c_str())));
    } else if (argument == "--section") {
      gallerySection = std::max(0, std::atoi(next().c_str()));
    } else if (argument == "--click-x") {
      clickX = static_cast<float>(std::atof(next().c_str()));
    } else if (argument == "--click-y") {
      clickY = static_cast<float>(std::atof(next().c_str()));
    } else if (argument == "--drag-scroll") {
      dragScroll = true;
    } else if (argument == "--click-twice") {
      clickTwice = true;
    } else if (argument == "--drag-row") {
      dragRow = true;
    } else if (argument == "--control") {
      galleryControl = next();
    } else if (argument == "--theme") {
      themeName = next();
    } else if (argument == "--color-space") {
      colorSpaceName = next();
    } else if (argument == "--help") {
      std::printf("usage: ui_demo [--headless] [--frames N] [--out FILE] [--width W] [--height H] "
        "[--platform auto|cocoa|x11|wayland] [--render-host same-thread|render-thread] "
        "[--scale S] [--theme dark|light] [--color-space srgb|p3|linear-p3|hdr10] "
        "[--focus-input] [--toolbar] [--native-disabled] [--ime-demo] [--acceptance] [--full-size] [--bookmark-smoke] [--bookmark-panel] [--section N] [--control NAME]\n");
      return 0;
    }
  }

  if (bookmarkPanel) {
    FileDialogOptions dialog;
    dialog.title = "Choose a file for the bookmark smoke";
    const FileDialogResult picked = showFileDialog(FileDialogKind::OpenFile, dialog);
    if (!picked.accepted || picked.paths.empty()) {
      std::fprintf(stderr, "bookmark-panel: cancelled\n");
      return 2;
    }
    std::string error;
    const auto bookmark = coral::ui::createSecurityScopedBookmark(picked.paths.front(), &error);
    if (!bookmark.has_value()) {
      std::fprintf(stderr, "bookmark-panel create failed: %s\n", error.c_str());
      return 1;
    }
    auto access = coral::ui::SecurityScopedAccess::fromBookmark(*bookmark, &error);
    if (!access.has_value() || !access->valid() || !access->startAccessing()) {
      std::fprintf(stderr, "bookmark-panel resolve/start failed: %s\n", error.c_str());
      return 1;
    }
    std::ifstream input(access->path(), std::ios::binary);
    const bool readable = input.good();
    access->stopAccessing();
    if (!readable) {
      std::fprintf(stderr, "bookmark-panel file was not readable\n");
      return 1;
    }
    std::printf("bookmark-panel=ok path=%s\n", access->path().c_str());
    return 0;
  }

  if (bookmarkSmoke) {
    const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "coral-bookmark-smoke.txt";
    {
      std::ofstream file(path);
      file << "coral bookmark smoke";
    }
    std::string error;
    const auto bookmark = coral::ui::createSecurityScopedBookmark(path.string(), &error);
    if (!bookmark.has_value()) {
      std::fprintf(stderr, "bookmark create failed: %s\n", error.c_str());
      return 1;
    }
    auto access = coral::ui::SecurityScopedAccess::fromBookmark(*bookmark, &error);
    if (!access.has_value() || !access->valid() || !access->startAccessing()) {
      std::fprintf(stderr, "bookmark resolve/start failed: %s\n", error.c_str());
      return 1;
    }
    std::ifstream input(access->path());
    std::string content;
    std::getline(input, content);
    access->stopAccessing();
    std::filesystem::remove(path);
    if (content != "coral bookmark smoke") {
      std::fprintf(stderr, "bookmark read mismatch: %s\n", content.c_str());
      return 1;
    }
    std::printf("bookmark-smoke=ok path=%s\n", access->path().c_str());
    return 0;
  }

  Window::Options options;
  options.width = width;
  options.height = height;
  options.headless = headless;
  options.title = "CoralUI control gallery";
  options.platform = platform;
  options.uiScale = uiScale;
  options.renderHost = threadedHost ? RenderHostKind::RenderThread : RenderHostKind::SameThread;
  options.fullSizeContentView = fullSizeContentView;
  if (colorSpaceName == "p3" || colorSpaceName == "display-p3") {
    options.outputColorSpace = OutputColorSpace::DisplayP3;
  } else if (colorSpaceName == "linear-p3" || colorSpaceName == "edr") {
    options.outputColorSpace = OutputColorSpace::ExtendedLinearP3;
  } else if (colorSpaceName == "hdr10" || colorSpaceName == "pq") {
    options.outputColorSpace = OutputColorSpace::Hdr10Pq;
  }
  options.frameLimit = headless && frames == 0 ? 1 : frames;
  auto window = Window::create(options);
  if (window == nullptr) {
    std::fprintf(stderr, "the UI window could not be created: %s\n",
      Window::lastCreationError().c_str());
    return 1;
  }

  // Keep --width/--height in logical units on every platform. Win32 and X11 interpret
  // Options::width as device pixels, so on a 150% display a 960x700 request would give the
  // fixed-size gallery a 640x467 logical viewport and clip its right-hand column; macOS points
  // already are logical units. The effective scale is only known after creation, so resize once
  // here and size the page from the window's logical geometry below.
  const float windowScale = window->uiScale() > 0.0f ? window->uiScale() : 1.0f;
  if (!headless && windowScale != 1.0f) {
    const int scaledWidth = static_cast<int>(std::lround(static_cast<float>(width) * windowScale));
    const int scaledHeight = static_cast<int>(std::lround(static_cast<float>(height) * windowScale));
    if (scaledWidth != window->width() || scaledHeight != window->height()) {
      (void)window->resize(scaledWidth, scaledHeight);
    }
  }

  if (showToolbar) {
    window->setToolbarStyle(Window::ToolbarStyle::Unified);
    std::vector<Window::ToolbarItem> items{
      { "com.coral.gallery.new", "New", "plus", "New item" },
      { "com.coral.gallery.open", "Open", "folder", "Open item" },
      { "com.coral.gallery.sep", "", "", "", nullptr, false, true },
      { "com.coral.gallery.flex", "", "", "", nullptr, true, false },
      { "com.coral.gallery.refresh", "Refresh", "arrow.clockwise", "Refresh" },
      { "com.coral.gallery.more", "More", "ellipsis.circle", "More" },
    };
    for (int index = 0; index < 12; ++index) {
      items.push_back({ "com.coral.gallery.extra" + std::to_string(index),
        "Action " + std::to_string(index), "circle", "Overflow acceptance item" });
    }
    window->setToolbarItems(std::move(items));
  }

  // Esc and Cmd+W close the demo on every platform. The focused control gets the key first, so a
  // text field can consume Esc to cancel its composition without the window disappearing.
  window->commands().bind(Shortcut{ Key::Escape, false, false, false, false }, "window.close",
    [w = window.get()] { w->requestClose(); });
  window->commands().bindAlias(Shortcut::command(Key::W), "window.close");

  // Manual acceptance shortcuts for the macOS platform services. Cmd+Shift+M/F/P/N/C/G/S/K.
  bool acceptanceMaterial = false;
  bool acceptanceFocusRing = false;
  window->commands().bind(Shortcut{ Key::M, false, true, false, true }, "acceptance.material",
    [w = window.get(), &acceptanceMaterial] {
      acceptanceMaterial = !acceptanceMaterial;
      w->setBackgroundMaterial(acceptanceMaterial
        ? Window::BackgroundMaterial::Sidebar : Window::BackgroundMaterial::None);
    });
  window->commands().bind(Shortcut{ Key::F, false, true, false, true }, "acceptance.focus-ring",
    [w = window.get(), &acceptanceFocusRing] {
      acceptanceFocusRing = !acceptanceFocusRing;
      w->setContentFocusRingEnabled(acceptanceFocusRing);
    });
  window->commands().bind(Shortcut{ Key::P, false, true, false, true }, "acceptance.print",
    [w = window.get()] { std::printf("printWindow=%d\n", w->printWindow() ? 1 : 0); });
  window->commands().bind(Shortcut{ Key::N, false, true, false, true }, "acceptance.notify",
    [w = window.get()] {
      coral::ui::requestNotificationAuthorization(
        [w](const coral::ui::NotificationAuthorization status) {
          std::printf("notification-authorization=%d\n", static_cast<int>(status));
          if (status == coral::ui::NotificationAuthorization::Authorized ||
              status == coral::ui::NotificationAuthorization::Provisional) {
            std::printf("postUserNotification=%d\n",
              coral::ui::postUserNotification("Coral UI", "Manual acceptance notification") ? 1 : 0);
          }
          (void)w;
        });
    });
  window->commands().bind(Shortcut{ Key::C, false, true, false, true }, "acceptance.capture",
    [w = window.get()] {
      graphics::Bitmap bitmap;
      std::printf("captureWindow=%d %dx%d\n", w->captureWindow(&bitmap) ? 1 : 0,
        bitmap.width(), bitmap.height());
    });
  window->commands().bind(Shortcut{ Key::G, false, true, false, true }, "acceptance.font-panel",
    [] {
      (void)coral::ui::showFontPanel([](std::shared_ptr<const graphics::Font> font) {
        if (font != nullptr) {
          std::printf("font-panel: face=%s size=%.1f\n",
            font->typeface()->faceName().c_str(), static_cast<double>(font->size()));
        }
      });
    });
  window->commands().bind(Shortcut{ Key::D, false, true, false, true }, "acceptance.stats",
    [w = window.get()] {
      const coral::ui::FrameStatistics stats = w->frameStatistics();
      std::printf("display: colorSpace=%d hdr=%d headroom=%.2f wideColor=%d refresh=%.1f\n",
        static_cast<int>(w->outputColorSpace()), stats.hdrOutput ? 1 : 0,
        static_cast<double>(w->displayHeadroom()), w->supportsWideColor() ? 1 : 0,
        static_cast<double>(stats.refreshHertz));
    });
  window->commands().bind(Shortcut{ Key::S, false, true, false, true }, "acceptance.share",
    [w = window.get()] { w->showShareSheet("Coral UI manual acceptance"); });
  window->commands().bind(Shortcut{ Key::K, false, true, false, true }, "acceptance.color-panel",
    [raw = window.get()] {
      // Keep the shortcut panel inside CoralUI's layer tree. AppKit's shared NSColorPanel is its own
      // window and does not follow the gallery's uiScale, which made its controls look half-sized on
      // a scaled display.
      auto picker = std::make_shared<coral::ui::ColorPicker>();
      picker->setMode(coral::ui::ColorPickerMode::Hsv);
      picker->setColor(graphics::Color::MakeRGBA8(86, 156, 255, 255));
      picker->setOnChanged([](const graphics::Color color) {
        std::printf("color-panel: %.3f %.3f %.3f %.3f\n",
          static_cast<double>(color.red), static_cast<double>(color.green),
          static_cast<double>(color.blue), static_cast<double>(color.alpha));
      });
      auto popup = std::make_shared<coral::ui::Popup>(picker);
      if (raw->root() != nullptr) {
        raw->root()->addChild(popup);
        popup->showAt(coral::ui::Point{ 260.0f, 220.0f });
      }
    });

  const auto typeface = graphics::Typeface::fromFile(fontPath());
  Theme theme = themeName == "light" ? Theme::light() : Theme::dark();
  if (typeface != nullptr) {
    theme.body = graphics::Font::create(typeface, 14.0f);
    theme.title = graphics::Font::create(typeface, 24.0f);
  }
  window->setTheme(theme);
  if (acceptance) {
    Theme translucent = theme;
    translucent.background = Color::MakeRGBA8(0, 0, 0, 0);
    window->setTheme(translucent);
    window->setClearColor(Color::MakeRGBA8(0, 0, 0, 0));
    window->setBackgroundMaterial(Window::BackgroundMaterial::Sidebar);
    window->setContentFocusRingEnabled(true);
  }

  std::shared_ptr<TextInput> inputField;
  auto root = std::make_shared<Layer>();
  if (acceptance) root->setBackgroundColor(Color::MakeRGBA8(0, 0, 0, 0));
  else root->setBackgroundColorRole(ThemeColorRole::Background);
  auto page = std::make_shared<GalleryPage>(window.get(), fullSizeContentView);
  // The layer tree lays out at width()/uiScale() logical units on every platform.
  page->setBounds(Rect::MakeWH(static_cast<float>(window->width()) / windowScale,
    static_cast<float>(window->height()) / windowScale));
  window->setSafeAreaChangedHandler([page](const Insets insets) {
    page->setTranslation({ 0.0f, insets.top });
  });
  root->addChild(page);
  buildSectionedGallery(page, theme, nativeDisabled, inputField, gallerySection,
    galleryControl);

  window->setRoot(root);
  if (clickX >= 0.0f && clickY >= 0.0f) {
    PointerEvent click;
    click.position = { clickX, clickY };
    click.button = PointerButton::Primary;
    window->pointerDown(click);
    window->pointerUp(click);
    if (clickTwice) {
      window->pump();
      window->pointerDown(click);
      window->pointerUp(click);
    }
  }
  if (dragRow) {
    PointerEvent hover;
    hover.position = { 30.0f, 313.0f };
    window->pointerMove(hover);
    PointerEvent down = hover;
    down.button = PointerButton::Primary;
    window->pointerDown(down);
    window->pump();
    PointerEvent drag = hover;
    drag.position = { 30.0f, 340.0f };
    window->pointerMove(drag);
    window->pointerUp(drag);
  }
  if (dragScroll) {
    PointerEvent down;
    down.position = { 214.0f, 200.0f };
    down.button = PointerButton::Primary;
    window->pointerDown(down);
    PointerEvent move = down;
    move.position = { 214.0f, 420.0f };
    window->pointerMove(move);
    window->pointerUp(move);
  }
  if (imeDemo && inputField != nullptr) {
    inputField->setText("中文输入\n第二行");
    window->focusLayer(inputField.get());
  } else if (focusInput && inputField != nullptr) {
    window->focusLayer(inputField.get());
  }
  const int rendered = window->run();
  if (rendered == 0) {
    std::fprintf(stderr, "the UI window never rendered: %s\n", window->diagnostics().c_str());
    return 1;
  }
  if (!headless && !window->isRenderTargetOnScreen()) {
    std::fprintf(stderr, "the render target is offscreen; frames and snapshots work but no window "
                         "presents them\n");
  }
  if (!window->diagnostics().empty()) {
    std::fprintf(stderr, "window diagnostics: %s\n", window->diagnostics().c_str());
  }

  if (!output.empty()) {
    graphics::Bitmap bitmap;
    if (!window->snapshot(&bitmap) || !writePng(bitmap, output)) {
      return 1;
    }
  }
  std::printf("%s %d frame(s), refresh %.1f Hz, platform=%s, dpr=%.2f, uiScale=%.2f\n",
    headless ? "headless" : "window", rendered, window->refreshHertz(),
    platformName(window->nativePlatform()), window->devicePixelRatio(), window->uiScale());
  return 0;
}

const char* platformName(const coral::ui::Window::NativePlatform platform) {
  switch (platform) {
    case coral::ui::Window::NativePlatform::Win32: return "win32";
    case coral::ui::Window::NativePlatform::X11: return "x11";
    case coral::ui::Window::NativePlatform::Wayland: return "wayland";
    case coral::ui::Window::NativePlatform::Cocoa: return "cocoa";
    case coral::ui::Window::NativePlatform::Auto: return "headless";
  }
  return "unknown";
}
