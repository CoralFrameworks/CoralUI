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

#include "../../WindowPlatform.h"
#if defined(CORAL_UI_HAS_ATSPI)
#include "AtspiBridge.h"
#endif
#include "ImeBackend.h"
#include "WakePipe.h"
#include "LinuxAppearance.h"

#include <gpu/WindowSurface.h>
#include <ui/ClipboardMime.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

// After the Xlib headers on purpose: everything above must be parsed before Xlib's `None` macro is
// defined, or backend enums such as `Feature::None` stop compiling.
#include "XdndSource.h"

#include <poll.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace coral::ui::detail {

namespace {

float queryDpiScale(Display* display) {
  if (display == nullptr) {
    return 1.0f;
  }
  // Xft.dpi is what most desktop environments set for HiDPI; the physical screen size is a
  // reasonable fallback for a bare X server.
  if (char* resources = XResourceManagerString(display); resources != nullptr) {
    XrmDatabase database = XrmGetStringDatabase(resources);
    if (database != nullptr) {
      XrmValue value{};
      char* type = nullptr;
      if (XrmGetResource(database, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr != nullptr) {
        char* end = nullptr;
        const double dpi = std::strtod(value.addr, &end);
        if (end != value.addr && dpi > 0.0) {
          XrmDestroyDatabase(database);
          return std::clamp(static_cast<float>(dpi / 96.0), 1.0f, 4.0f);
        }
      }
      XrmDestroyDatabase(database);
    }
  }
  const int screen = DefaultScreen(display);
  const int millimeters = DisplayWidthMM(display, screen);
  const int pixels = DisplayWidth(display, screen);
  if (millimeters > 0 && pixels > 0) {
    return std::clamp(static_cast<float>(pixels) * 25.4f /
      (static_cast<float>(millimeters) * 96.0f), 1.0f, 4.0f);
  }
  return 1.0f;
}

Key keyFromKeysym(const KeySym symbol) {
  if (symbol >= XK_a && symbol <= XK_z) {
    return static_cast<Key>(static_cast<uint16_t>(Key::A) + static_cast<uint16_t>(symbol - XK_a));
  }
  if (symbol >= XK_A && symbol <= XK_Z) {
    return static_cast<Key>(static_cast<uint16_t>(Key::A) + static_cast<uint16_t>(symbol - XK_A));
  }
  if (symbol >= XK_0 && symbol <= XK_9) {
    return static_cast<Key>(static_cast<uint16_t>(Key::Digit0) +
      static_cast<uint16_t>(symbol - XK_0));
  }
  if (symbol >= XK_F1 && symbol <= XK_F12) {
    return static_cast<Key>(static_cast<uint16_t>(Key::F1) +
      static_cast<uint16_t>(symbol - XK_F1));
  }
  switch (symbol) {
    case XK_Escape: return Key::Escape;
    case XK_Return: return Key::Enter;
    case XK_space: return Key::Space;
    case XK_Tab: return Key::Tab;
    case XK_BackSpace: return Key::Backspace;
    case XK_Left: return Key::Left;
    case XK_Right: return Key::Right;
    case XK_Up: return Key::Up;
    case XK_Down: return Key::Down;
    case XK_Delete: return Key::Delete;
    case XK_Home: return Key::Home;
    case XK_End: return Key::End;
    case XK_Prior: return Key::PageUp;
    case XK_Next: return Key::PageDown;
    case XK_Insert: return Key::Insert;
    case XK_comma: return Key::Comma;
    case XK_period: return Key::Period;
    case XK_slash: return Key::Slash;
    case XK_backslash: return Key::Backslash;
    case XK_semicolon: return Key::Semicolon;
    case XK_apostrophe: return Key::Apostrophe;
    case XK_bracketleft: return Key::LeftBracket;
    case XK_bracketright: return Key::RightBracket;
    case XK_minus: return Key::Minus;
    case XK_equal: return Key::Equal;
    case XK_grave: return Key::Grave;
    case XK_Shift_L: return Key::LeftShift;
    case XK_Shift_R: return Key::RightShift;
    case XK_Control_L: return Key::LeftControl;
    case XK_Control_R: return Key::RightControl;
    case XK_Alt_L: return Key::LeftAlt;
    case XK_Alt_R: return Key::RightAlt;
    case XK_Meta_L: return Key::LeftMeta;
    case XK_Meta_R: return Key::RightMeta;
    // Super is the Windows/Command key; report it as the platform's meta.
    case XK_Super_L: return Key::LeftMeta;
    case XK_Super_R: return Key::RightMeta;
    case XK_Caps_Lock: return Key::CapsLock;
    case XK_Num_Lock: return Key::NumLock;
    case XK_Scroll_Lock: return Key::ScrollLock;
    case XK_Print: return Key::PrintScreen;
    case XK_Pause: return Key::Pause;
    case XK_Menu: return Key::Menu;
    case XK_KP_0:
    case XK_KP_1:
    case XK_KP_2:
    case XK_KP_3:
    case XK_KP_4:
    case XK_KP_5:
    case XK_KP_6:
    case XK_KP_7:
    case XK_KP_8:
    case XK_KP_9:
      return static_cast<Key>(static_cast<uint16_t>(Key::Numpad0) +
        static_cast<uint16_t>(symbol - XK_KP_0));
    case XK_KP_Multiply: return Key::NumpadMultiply;
    case XK_KP_Add: return Key::NumpadAdd;
    case XK_KP_Subtract: return Key::NumpadSubtract;
    case XK_KP_Decimal: return Key::NumpadDecimal;
    case XK_KP_Divide: return Key::NumpadDivide;
    case XK_KP_Enter: return Key::NumpadEnter;
    case XK_KP_Equal: return Key::NumpadEqual;
    default: return Key::Unknown;
  }
}

std::string uriToPath(std::string uri) {
  if (uri.rfind("file://", 0) == 0) uri.erase(0, 7);
  if (!uri.empty() && uri.front() != '/') {
    const size_t slash = uri.find('/');
    if (slash == std::string::npos) return {};
    uri.erase(0, slash);
  }
  std::string result;
  for (size_t index = 0; index < uri.size(); ++index) {
    if (uri[index] == '%' && index + 2 < uri.size() &&
        std::isxdigit(static_cast<unsigned char>(uri[index + 1])) != 0 &&
        std::isxdigit(static_cast<unsigned char>(uri[index + 2])) != 0) {
      const auto hex = [](const char c) {
        return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10
          : c - 'A' + 10);
      };
      result.push_back(static_cast<char>((hex(uri[index + 1]) << 4) | hex(uri[index + 2])));
      index += 2;
      continue;
    }
    result.push_back(uri[index]);
  }
  return result;
}

std::vector<std::string> parseUriList(const std::string& text) {
  std::vector<std::string> paths;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find_first_of("\r\n", start);
    std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!line.empty() && line.front() != '#') {
      const std::string path = uriToPath(line);
      if (!path.empty()) paths.push_back(path);
    }
    if (end == std::string::npos) break;
    start = text.find_first_not_of("\r\n", end);
    if (start == std::string::npos) break;
  }
  return paths;
}

} // namespace anonymous

class X11Platform final : public WindowPlatform, public ImeHost {
 public:
  X11Platform(Window::Options options, backend::Device* sharedDevice)
      : mOptions(std::move(options)), mSharedDevice(sharedDevice), mWidth(options.width),
        mHeight(options.height) {}

  ~X11Platform() override {
    mSurface.reset();
    mIme.reset();
    mWake.close();
    if (mDisplay != nullptr) {
      for (Cursor& cursor: mCursors) {
        if (cursor != 0) XFreeCursor(mDisplay, cursor);
      }
      if (mWindow != 0) XDestroyWindow(mDisplay, mWindow);
      XCloseDisplay(mDisplay);
    }
  }

  bool initialize(std::string* diagnostics) {
    // The phase-2 render thread creates the window surface with this same Display while the UI
    // thread pumps events; Xlib needs its process-wide locks initialized before the first request.
    static const bool threadsInitialized = [] {
      XInitThreads();
      return true;
    }();
    (void)threadsInitialized;
    mDisplay = XOpenDisplay(nullptr);
    if (mDisplay == nullptr) {
      if (diagnostics != nullptr) *diagnostics = "the X display could not be opened";
      return false;
    }
    mWake.open();
    mDpiScale = queryDpiScale(mDisplay);
    const int screen = DefaultScreen(mDisplay);
    mWindow = XCreateSimpleWindow(mDisplay, RootWindow(mDisplay, screen), 0, 0,
      static_cast<unsigned int>(mWidth), static_cast<unsigned int>(mHeight), 0,
      BlackPixel(mDisplay, screen), BlackPixel(mDisplay, screen));
    XStoreName(mDisplay, mWindow, mOptions.title.c_str());
    XClassHint classHint{};
    classHint.res_name = const_cast<char*>(mOptions.appId.c_str());
    classHint.res_class = const_cast<char*>(mOptions.appId.c_str());
    XSetClassHint(mDisplay, mWindow, &classHint);
    XSelectInput(mDisplay, mWindow, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
      ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | FocusChangeMask |
      LeaveWindowMask | PropertyChangeMask);
    mNetWmState = XInternAtom(mDisplay, "_NET_WM_STATE", False);
    mMaximizedVert = XInternAtom(mDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    mMaximizedHorz = XInternAtom(mDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    mAbove = XInternAtom(mDisplay, "_NET_WM_STATE_ABOVE", False);
    mHidden = XInternAtom(mDisplay, "_NET_WM_STATE_HIDDEN", False);
    mModalAtom = XInternAtom(mDisplay, "_NET_WM_STATE_MODAL", False);
    mMotifHints = XInternAtom(mDisplay, "_MOTIF_WM_HINTS", False);
    mOpacityAtom = XInternAtom(mDisplay, "_NET_WM_WINDOW_OPACITY", False);
    createCursors();
    mDeleteAtom = XInternAtom(mDisplay, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(mDisplay, mWindow, &mDeleteAtom, 1);
    mXdndAware = XInternAtom(mDisplay, "XdndAware", False);
    mXdndEnter = XInternAtom(mDisplay, "XdndEnter", False);
    mXdndPosition = XInternAtom(mDisplay, "XdndPosition", False);
    mXdndStatus = XInternAtom(mDisplay, "XdndStatus", False);
    mXdndLeave = XInternAtom(mDisplay, "XdndLeave", False);
    mXdndDrop = XInternAtom(mDisplay, "XdndDrop", False);
    mXdndFinished = XInternAtom(mDisplay, "XdndFinished", False);
    mXdndSelection = XInternAtom(mDisplay, "XdndSelection", False);
    mXdndActionCopy = XInternAtom(mDisplay, "XdndActionCopy", False);
    mUriList = XInternAtom(mDisplay, "text/uri-list", False);
    mTextHtml = XInternAtom(mDisplay, "text/html", False);
    mImagePng = XInternAtom(mDisplay, "image/png", False);
    mUtf8String = XInternAtom(mDisplay, "UTF8_STRING", False);
    mIme = nullptr;
    const auto envContains = [](const char* name, const char* needle) {
      const char* value = std::getenv(name);
      return value != nullptr && std::string(value).find(needle) != std::string::npos;
    };
    const char* requested = std::getenv("CORAL_IME");
    const bool wantFcitx = requested != nullptr && std::string(requested) == "fcitx";
    const bool wantXim = requested != nullptr && std::string(requested) == "xim";
    const bool modifiersSayFcitx = envContains("XMODIFIERS", "fcitx");
    const bool forceXim = wantFcitx || wantXim || modifiersSayFcitx;
#if defined(CORAL_UI_HAS_IBUS)
    // Auto prefers IBus when it connects: both IBus and Fcitx5's ibusfrontend expose the same
    // org.freedesktop.IBus service, which is the only path that gives the UI a lookup table.
    if (!forceXim) mIme = makeIbusIme(mDisplay, static_cast<unsigned long>(mWindow), this);
#endif
    if (mIme == nullptr) {
      // XOpenIM follows XMODIFIERS, so this is the Fcitx/XIM fallback; Fcitx draws its own candidate
      // window on this path.
      mIme = makeXimIme(mDisplay, static_cast<unsigned long>(mWindow), this);
    }
    mDropProperty = XInternAtom(mDisplay, "CORAL_DROP", False);
    const Atom version = 4;
    XChangeProperty(mDisplay, mWindow, mXdndAware, XA_ATOM, 32, PropModeReplace,
      reinterpret_cast<const unsigned char*>(&version), 1);
    XMapWindow(mDisplay, mWindow);
    XFlush(mDisplay);
    if (mOptions.renderHost != RenderHostKind::RenderThread && !createSurface()) {
      if (diagnostics != nullptr) *diagnostics = "the GPU X11 window surface could not be created";
      return false;
    }
    return true;
  }

  [[nodiscard]] Window::NativePlatform kind() const override {
    return Window::NativePlatform::X11;
  }
  [[nodiscard]] bool headless() const override {
    return false;
  }
  [[nodiscard]] int width() const override {
    return mWidth;
  }
  [[nodiscard]] int height() const override {
    return mHeight;
  }
  [[nodiscard]] float dpiScale() const override {
    return mDpiScale;
  }

  bool pump(WindowHost& host) override {
#if defined(CORAL_UI_HAS_ATSPI)
    if (mAccessibility) mAccessibility->pump();
#endif
    mHost = &host;
    if (mDisplay == nullptr) return false;
    while (XPending(mDisplay) > 0) {
      XEvent event{};
      XNextEvent(mDisplay, &event);
      handleEvent(event);
    }
    return true;
  }

  void waitForEvents(const int timeoutMs) override {
    if (mDisplay == nullptr) return;
    pollfd descriptors[2]{
      { ConnectionNumber(mDisplay), POLLIN, 0 },
      { mWake.readFd(), POLLIN, 0 },
    };
    const nfds_t count = mWake.readFd() >= 0 ? 2 : 1;
    if (poll(descriptors, count, timeoutMs) > 0 && count == 2 &&
        (descriptors[1].revents & POLLIN) != 0) {
      mWake.drain();
    }
  }

  void wake() override { mWake.wake(); }

  [[nodiscard]] graphics::Canvas* beginFrame() override {
    if (mSurface == nullptr) return nullptr;
    // A configure can arrive after a programmatic resize or from the window manager itself; reconcile
    // here, exactly as the Win32 platform does, so the swapchain never trails the client area.
    if (mSurface->width() != static_cast<uint32_t>(mWidth) ||
        mSurface->height() != static_cast<uint32_t>(mHeight)) {
      if (!mSurface->resize(static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight))) {
        mDiagnostics = mSurface->diagnostics();
        return nullptr;
      }
    }
    return mSurface->beginFrame();
  }
  bool present() override {
    return mSurface != nullptr && mSurface->present();
  }
  [[nodiscard]] bool supportsRetainedFrames() const override { return true; }
  bool presentRetained() override {
    return mSurface != nullptr && mSurface->presentRetained();
  }
  [[nodiscard]] bool supportsDamageRects() const override { return true; }
  bool presentDamage(const Rect& damage) override {
    return mSurface != nullptr && mSurface->presentDamage(damage);
  }
  /** Multi-list window frames, behind the same opt-in as the Win32 window path. */
  [[nodiscard]] bool supportsBoundarySubmission() const override {
    static const bool enabled = std::getenv("CORAL_UI_FRAME_PLAN_WINDOW") != nullptr;
    return enabled && mSurface != nullptr;
  }
  bool presentBoundaries(const std::vector<graphics::gpu::Surface::ListItem>& items) override {
    if (mSurface == nullptr) return false;
    std::vector<graphics::gpu::WindowSurface::ListItem> windowItems;
    windowItems.reserve(items.size());
    for (const graphics::gpu::Surface::ListItem& item: items) {
      graphics::gpu::WindowSurface::ListItem windowItem;
      windowItem.list = item.list;
      windowItem.clip = item.clip;
      windowItem.translation = item.translation;
      windowItem.opacity = item.opacity;
      windowItems.push_back(std::move(windowItem));
    }
    return mSurface->presentListItems(windowItems);
  }
  [[nodiscard]] uint32_t lastFrameCommandBuffers() const override {
    return mSurface != nullptr ? mSurface->lastFrameSubmitCount() : 0;
  }
  [[nodiscard]] uint32_t lastFrameDrawCalls() const override {
    return mSurface != nullptr ? mSurface->lastFrameDrawCallCount() : 0;
  }
  [[nodiscard]] uint32_t lastFrameBatches() const override {
    return mSurface != nullptr ? mSurface->lastFrameBatchCount() : 0;
  }

  [[nodiscard]] bool snapshot(graphics::Bitmap* out) override {
    if (mSurface == nullptr) {
      mDiagnostics = "no window surface";
      return false;
    }
    const bool read = mSurface->readback(out);
    if (!read) mDiagnostics = mSurface->diagnostics();
    return read;
  }
  bool resize(uint32_t width, uint32_t height) override {
    if (mOptions.renderHost != RenderHostKind::RenderThread && mSurface == nullptr) {
      mDiagnostics = "no window surface";
      return false;
    }
    if (mDisplay != nullptr && mWindow != 0) {
      // Resize the window first, then read back the size the server actually granted. Resizing the
      // swapchain from the requested size leaves it one configure behind when a WM clamps the window.
      XResizeWindow(mDisplay, mWindow, width, height);
      XSync(mDisplay, False);
      XWindowAttributes attributes{};
      if (XGetWindowAttributes(mDisplay, mWindow, &attributes) != 0 &&
          attributes.width > 0 && attributes.height > 0) {
        width = static_cast<uint32_t>(attributes.width);
        height = static_cast<uint32_t>(attributes.height);
      }
      XFlush(mDisplay);
    }
    if (mOptions.renderHost == RenderHostKind::RenderThread) {
      mWidth = static_cast<int>(width);
      mHeight = static_cast<int>(height);
      return true;
    }
    if (!mSurface->resize(width, height)) {
      mDiagnostics = mSurface->diagnostics();
      return false;
    }
    mWidth = static_cast<int>(width);
    mHeight = static_cast<int>(height);
    return true;
  }
  bool recreate() override {
    if (mOptions.renderHost == RenderHostKind::RenderThread) return true;
    mSurface.reset();
    return createSurface();
  }
  [[nodiscard]] std::string diagnostics() const override {
    return mDiagnostics;
  }
  [[nodiscard]] graphics::gpu::NativeDisplay nativeDisplay() const override {
    graphics::gpu::NativeDisplay native;
    native.kind = graphics::gpu::NativeDisplayKind::X11;
    native.display = mDisplay;
    native.surface = reinterpret_cast<void*>(static_cast<uintptr_t>(mWindow));
    return native;
  }

  [[nodiscard]] bool prefersDarkAppearance() const override { return linuxPrefersDark(); }

  [[nodiscard]] SystemAppearance systemAppearance() const override {
    return linuxSystemAppearance();
  }

  void setAccessibilityTreeProvider(std::function<AccessibilityNode()> provider,
      AccessibilityActionInvoker invoker = {}) override {
#if defined(CORAL_UI_HAS_ATSPI)
    mAccessibility = detail::createAtspiBridge(std::move(provider), std::move(invoker), mTitle);
#else
    (void)provider;
    (void)invoker;
#endif
  }

  void raiseAccessibilityFocusEvent() override {
#if defined(CORAL_UI_HAS_ATSPI)
    if (mAccessibility) mAccessibility->raiseFocusEvent();
#endif
  }

  void raiseAccessibilityPropertyEvent(const std::string_view path,
      const AccessibilityEvent event) override {
#if defined(CORAL_UI_HAS_ATSPI)
    if (mAccessibility) mAccessibility->raisePropertyEvent(path, event);
#else
    (void)path;
    (void)event;
#endif
  }

  void setTitle(std::string_view title) override {
    mTitle = std::string(title);
#if defined(CORAL_UI_HAS_ATSPI)
    if (mAccessibility) mAccessibility->setTitle(mTitle);
#endif
    if (mDisplay == nullptr || mWindow == 0) return;
    const std::string text(title);
    XStoreName(mDisplay, mWindow, text.c_str());
    const Atom utf8 = XInternAtom(mDisplay, "UTF8_STRING", False);
    const Atom netName = XInternAtom(mDisplay, "_NET_WM_NAME", False);
    XChangeProperty(mDisplay, mWindow, netName, utf8, 8, PropModeReplace,
      reinterpret_cast<const unsigned char*>(text.data()), static_cast<int>(text.size()));
    XFlush(mDisplay);
  }

  void setPosition(const int x, const int y) override {
    if (mDisplay == nullptr || mWindow == 0) return;
    XMoveWindow(mDisplay, mWindow, x, y);
    XFlush(mDisplay);
  }

  void setMaximized(const bool maximized) override {
    sendNetWmState(mMaximizedVert, mMaximizedHorz, maximized);
  }

  void setTransientFor(WindowPlatform* parent) override {
    if (mDisplay == nullptr || mWindow == 0) return;
    auto* x11 = dynamic_cast<X11Platform*>(parent);
    const ::Window parentWindow = x11 != nullptr ? x11->mWindow : 0;
    XSetTransientForHint(mDisplay, mWindow, parentWindow);
    XFlush(mDisplay);
  }

  void setModal(const bool modal) override {
    sendNetWmState(mModalAtom, 0, modal);
  }

  void setMinimized(const bool minimized) override {
    if (mDisplay == nullptr || mWindow == 0) return;
    if (minimized) {
      XIconifyWindow(mDisplay, mWindow, DefaultScreen(mDisplay));
    } else {
      XMapRaised(mDisplay, mWindow);
      raise();
    }
    XFlush(mDisplay);
  }

  void setOpacity(const float opacity) override {
    if (mDisplay == nullptr || mWindow == 0 || mOpacityAtom == 0) return;
    const auto value = static_cast<unsigned long>(std::clamp(opacity, 0.0f, 1.0f) * 0xFFFFFFFFu);
    XChangeProperty(mDisplay, mWindow, mOpacityAtom, XA_CARDINAL, 32, PropModeReplace,
      reinterpret_cast<const unsigned char*>(&value), 1);
    XFlush(mDisplay);
  }

  void setAlwaysOnTop(const bool onTop) override {
    sendNetWmState(mAbove, 0, onTop);
  }

  void setPointerCapture(const bool captured) override {
    if (mDisplay == nullptr || mWindow == 0) return;
    if (captured) {
      XGrabPointer(mDisplay, mWindow, True,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync,
        None, None, CurrentTime);
    } else {
      XUngrabPointer(mDisplay, CurrentTime);
    }
    XFlush(mDisplay);
  }

  void setCursor(const Window::Cursor cursor) override {
    if (mDisplay == nullptr || mWindow == 0) return;
    const size_t index = static_cast<size_t>(cursor);
    if (index >= mCursors.size() || mCursors[index] == 0) return;
    XDefineCursor(mDisplay, mWindow, mCursors[index]);
    XFlush(mDisplay);
  }

  void setDecorations(const Window::Decorations decorations) override {
    if (mDisplay == nullptr || mWindow == 0) return;
    // MWM_HINTS_DECORATIONS (1 << 1): 0 asks the WM for a borderless window, 1 restores its frame.
    struct MotifHints {
      unsigned long flags;
      unsigned long functions;
      unsigned long decorations;
      long inputMode;
      unsigned long status;
    } hints{};
    hints.flags = 1u << 1;
    hints.decorations = decorations == Window::Decorations::Client ? 0u : 1u;
    XChangeProperty(mDisplay, mWindow, mMotifHints, mMotifHints, 32, PropModeReplace,
      reinterpret_cast<const unsigned char*>(&hints), 5);
    XFlush(mDisplay);
  }

  void setMinimumSize(const int width, const int height) override {
    if (mDisplay == nullptr || mWindow == 0) return;
    XSizeHints hints{};
    hints.flags = PMinSize;
    hints.min_width = std::max(1, width);
    hints.min_height = std::max(1, height);
    XSetWMNormalHints(mDisplay, mWindow, &hints);
    XFlush(mDisplay);
  }

  void setFullscreen(const bool fullscreen) override {
    if (mDisplay == nullptr || mWindow == 0) return;
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = mWindow;
    event.xclient.message_type = XInternAtom(mDisplay, "_NET_WM_STATE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = fullscreen ? 1 : 0; // _NET_WM_STATE_ADD / _NET_WM_STATE_REMOVE
    event.xclient.data.l[1] = static_cast<long>(XInternAtom(mDisplay, "_NET_WM_STATE_FULLSCREEN", False));
    event.xclient.data.l[3] = 1; // source indication: a normal application
    XSendEvent(mDisplay, DefaultRootWindow(mDisplay), False,
      SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(mDisplay);
  }

  [[nodiscard]] Rect screenBounds() const override {
    if (mDisplay == nullptr || mWindow == 0) return Rect{};
    XWindowAttributes attributes{};
    if (XGetWindowAttributes(mDisplay, mWindow, &attributes) == 0) return Rect{};
    ::Window child = 0;
    int rootX = 0;
    int rootY = 0;
    XTranslateCoordinates(mDisplay, mWindow, attributes.root, 0, 0, &rootX, &rootY, &child);
    return Rect::MakeLTRB(static_cast<float>(rootX), static_cast<float>(rootY),
      static_cast<float>(rootX + attributes.width), static_cast<float>(rootY + attributes.height));
  }

  void raise() override {
    if (mDisplay == nullptr || mWindow == 0) return;
    XRaiseWindow(mDisplay, mWindow);
    // Ask the window manager to make this the active window. Without _NET_ACTIVE_WINDOW the WM may
    // raise the window but leave keyboard focus where it was.
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = mWindow;
    event.xclient.message_type = XInternAtom(mDisplay, "_NET_ACTIVE_WINDOW", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // source indication: a normal application
    event.xclient.data.l[1] = CurrentTime;
    event.xclient.data.l[2] = 0;
    XSendEvent(mDisplay, DefaultRootWindow(mDisplay), False,
      SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(mDisplay);
  }

  /** The X11 window handle a render thread uses to create its window surface. */
  [[nodiscard]] void* nativeWindow() const override {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(mWindow));
  }

  [[nodiscard]] std::string portalParentHandle() const override {
    return mWindow == 0 ? std::string{} : "x11:" + std::to_string(mWindow);
  }

  [[nodiscard]] backend::Device* device() const override {
    return mSurface != nullptr ? mSurface->device() : nullptr;
  }

  [[nodiscard]] backend::Device* releaseDevice() override {
    return mSurface != nullptr ? mSurface->releaseDevice() : nullptr;
  }

  [[nodiscard]] uint32_t sampleCount() const override {
    return mSurface != nullptr ? mSurface->sampleCount() : 1;
  }

  /** The renderer's last-frame stages; the window surface owns the renderer that produced them. */
  [[nodiscard]] FrameRenderTiming lastFrameRenderTiming() const override {
    if (mSurface == nullptr) return {};
    const graphics::gpu::FrameStages stages = mSurface->lastFrameStages();
    return FrameRenderTiming{ stages.record, stages.execute, mSurface->lastFrameGpuMilliseconds(),
      stages.total, mSurface->residentBytes() };
  }

  /** True when the granted swapchain presents PQ numbers rather than sRGB. */
  [[nodiscard]] bool outputIsHdr() const override {
    return mSurface != nullptr &&
      mSurface->displayColorSpace() == backend::SwapchainColorSpace::Hdr10Pq;
  }

  void updateImeCursorRect(const Rect& rect) override {
    if (mIme == nullptr) return;
    refreshImeSurroundingText();
    // IBus wants screen coordinates in device pixels; `Rect` here is window-local logical units.
    const Rect origin = screenBounds();
    const Rect screen = Rect::MakeLTRB(origin.getLeft() + rect.getLeft() * mDpiScale,
      origin.getTop() + rect.getTop() * mDpiScale,
      origin.getLeft() + rect.getRight() * mDpiScale,
      origin.getTop() + rect.getBottom() * mDpiScale);
    mIme->setCursorRect(screen);
  }

  /**
   * Hands the drag to the X11 desktop.
   *
   * This is the XDND counterpart of OLE's `DoDragDrop`: it blocks on the UI thread while the pointer
   * is grabbed, so it must be called from the press/move gesture (which is where
   * `Layer::setDragOutProvider` lands), not from a frame callback.
   */
  DragEffect beginDragOut(const DragPayload& payload, const DragEffect allowed) override {
    XdndSource source(mDisplay, mWindow);
    return source.run(payload, allowed);
  }

  bool dispatchImeKey(const uint32_t key) override {
    return mIme != nullptr && mIme->handleKey(key);
  }

 private:
  /** Adds or removes one `_NET_WM_STATE` pair through a root-window client message. */
  /** Reads `_NET_WM_STATE` and tells the window what the WM actually applied. */
  void readWindowState() {
    if (mDisplay == nullptr || mWindow == 0 || mHost == nullptr) return;
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(mDisplay, mWindow, mNetWmState, 0, 64, False, XA_ATOM, &type,
        &format, &count, &remaining, &data) != Success || data == nullptr) {
      return;
    }
    bool maximized = false;
    bool minimized = false;
    bool fullscreen = false;
    if (format == 32) {
      const auto* atoms = reinterpret_cast<const Atom*>(data);
      for (unsigned long index = 0; index < count; ++index) {
        if (atoms[index] == mMaximizedVert || atoms[index] == mMaximizedHorz) maximized = true;
        if (atoms[index] == mHidden) minimized = true;
        if (atoms[index] == XInternAtom(mDisplay, "_NET_WM_STATE_FULLSCREEN", False)) fullscreen = true;
      }
    }
    XFree(data);
    mHost->platformStateChanged(maximized, minimized, fullscreen);
  }

  void sendNetWmState(const Atom first, const Atom second, const bool enable) {
    if (mDisplay == nullptr || mWindow == 0) return;
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = mWindow;
    event.xclient.message_type = mNetWmState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = enable ? 1 : 0; // _NET_WM_STATE_ADD / _NET_WM_STATE_REMOVE
    event.xclient.data.l[1] = static_cast<long>(first);
    event.xclient.data.l[2] = static_cast<long>(second);
    event.xclient.data.l[3] = 1; // source indication: a normal application
    XSendEvent(mDisplay, DefaultRootWindow(mDisplay), False,
      SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(mDisplay);
  }

  void refreshImeSurroundingText() {
    if (mHost == nullptr || mIme == nullptr) return;
    std::string text;
    size_t cursor = 0;
    size_t anchor = 0;
    if (!mHost->surroundingText(&text, &cursor, &anchor)) {
      text.clear();
      cursor = 0;
      anchor = 0;
    }
    if (text == mLastSurroundingText && cursor == mLastSurroundingCursor &&
        anchor == mLastSurroundingAnchor) {
      return;
    }
    mLastSurroundingText = text;
    mLastSurroundingCursor = cursor;
    mLastSurroundingAnchor = anchor;
    mIme->setSurroundingText(text, cursor, anchor);
  }

  void createCursors() {
    const std::array<unsigned int, 9> shapes = {
      XC_left_ptr, XC_xterm, XC_hand2, XC_sb_h_double_arrow, XC_sb_v_double_arrow,
      XC_fleur, XC_crosshair, XC_X_cursor, XC_watch,
    };
    for (size_t index = 0; index < shapes.size(); ++index) {
      mCursors[index] = XCreateFontCursor(mDisplay, shapes[index]);
    }
    if (mCursors[0] != 0) XDefineCursor(mDisplay, mWindow, mCursors[0]);
  }

  void onImeComposition(const std::string& text) override {
    if (mHost != nullptr) mHost->dispatchComposition(text);
  }
  void onImeCommit(const std::string& text) override {
    if (mHost != nullptr) mHost->dispatchCommit(text);
  }
  void onImeCandidates(std::vector<std::string> candidates, const size_t selected) override {
    if (mHost != nullptr) mHost->dispatchCandidates(std::move(candidates), selected);
  }
  [[nodiscard]] Rect imeCursorRect() const override {
    return mHost != nullptr ? mHost->imeCursorRect() : Rect{};
  }

  void sendXdndStatus(const bool accept) {
    if (mXdndSource == 0) return;
    XEvent reply{};
    reply.xclient.type = ClientMessage;
    reply.xclient.display = mDisplay;
    reply.xclient.window = mXdndSource;
    reply.xclient.message_type = mXdndStatus;
    reply.xclient.format = 32;
    reply.xclient.data.l[0] = static_cast<long>(mWindow);
    reply.xclient.data.l[1] = accept ? 1 : 0;
    reply.xclient.data.l[4] = static_cast<long>(mXdndActionCopy);
    XSendEvent(mDisplay, mXdndSource, False, NoEventMask, &reply);
    XFlush(mDisplay);
  }

  void finishXdnd(const bool success) {
    if (mXdndSource == 0) return;
    XEvent reply{};
    reply.xclient.type = ClientMessage;
    reply.xclient.display = mDisplay;
    reply.xclient.window = mXdndSource;
    reply.xclient.message_type = mXdndFinished;
    reply.xclient.format = 32;
    reply.xclient.data.l[0] = static_cast<long>(mWindow);
    reply.xclient.data.l[1] = success ? 1 : 0;
    reply.xclient.data.l[2] = static_cast<long>(mXdndActionCopy);
    XSendEvent(mDisplay, mXdndSource, False, NoEventMask, &reply);
    XFlush(mDisplay);
    mXdndSource = 0;
  }

  /**
   * The most useful of the targets the drag source advertised.
   *
   * A file drop is the most common cross-application drag, so `text/uri-list` wins; then the richer
   * text formats, then plain text. XDND only lets a target ask for one type per drop, so the others
   * are unreachable here (a source that wants several representations advertises them in turn).
   */
  [[nodiscard]] Atom preferredXdndTarget() const {
    for (const Atom candidate: { mUriList, mImagePng, mTextHtml, mUtf8String }) {
      if (candidate == None) continue;
      if (std::find(mXdndTypes.begin(), mXdndTypes.end(), candidate) != mXdndTypes.end()) {
        return candidate;
      }
    }
    return None;
  }

  /**
   * A hover event carrying the type of the drop without the bytes.
   *
   * XDND only transfers data on the drop, but a layer decides whether it is a target from the MIME
   * type, so the enter/over events carry the payload type with empty data. The drop then arrives with
   * the real bytes through `DropEvent`.
   */
  [[nodiscard]] DragEvent makeXdndDragEvent() const {
    DragEvent event;
    event.position = mDropPosition;
    event.external = true;
    const std::string mime = mimeForSelectionTarget(atomName(preferredXdndTarget()));
    if (mime == "text/uri-list") {
      event.payload = DragPayload::files({});
    } else if (!mime.empty()) {
      event.payload = DragPayload::custom(mime, {});
    }
    return event;
  }

  void handleXdnd(const XClientMessageEvent& message) {
    if (message.message_type == mXdndEnter) {
      mXdndSource = static_cast<::Window>(message.data.l[0]);
      // Version 5 carries up to three type atoms in the message; a longer list lives in the source's
      // XdndTypeList property and is not read here (three covers the formats this backend accepts).
      mXdndTypes.clear();
      for (int index = 2; index < 5; ++index) {
        const auto atom = static_cast<Atom>(message.data.l[index]);
        if (atom != None) mXdndTypes.push_back(atom);
      }
      mXdndAccepted = preferredXdndTarget() != None;
      sendXdndStatus(mXdndAccepted);
    } else if (message.message_type == mXdndPosition) {
      if (mXdndSource == 0) mXdndSource = static_cast<::Window>(message.data.l[0]);
      const int rootX = static_cast<short>((message.data.l[2] >> 16) & 0xFFFF);
      const int rootY = static_cast<short>(message.data.l[2] & 0xFFFF);
      int windowX = rootX;
      int windowY = rootY;
      ::Window child = 0;
      XTranslateCoordinates(mDisplay, DefaultRootWindow(mDisplay), mWindow, rootX, rootY,
        &windowX, &windowY, &child);
      mDropPosition = { static_cast<float>(windowX), static_cast<float>(windowY) };
      mXdndAccepted = preferredXdndTarget() != None;
      if (mXdndAccepted && mHost != nullptr) {
        // XdndEnter carries no coordinates, so the first position is where the hover is dispatched:
        // the layer under the pointer decides, and later positions ask the same target to update.
        mXdndAccepted = (mXdndHovered ? mHost->dispatchDragOver(makeXdndDragEvent())
                                      : mHost->dispatchDragEnter(makeXdndDragEvent())) !=
          kNoDragEffect;
        mXdndHovered = true;
      }
      sendXdndStatus(mXdndAccepted);
    } else if (message.message_type == mXdndDrop) {
      if (mXdndSource == 0) return;
      const Time time = static_cast<Time>(message.data.l[2]);
      mXdndTarget = preferredXdndTarget();
      if (mXdndTarget == None || !mXdndAccepted) {
        if (mHost != nullptr) mHost->dispatchDragLeave();
        mXdndHovered = false;
        finishXdnd(false);
        return;
      }
      XConvertSelection(mDisplay, mXdndSelection, mXdndTarget, mDropProperty, mWindow, time);
      XFlush(mDisplay);
    } else if (message.message_type == mXdndLeave) {
      if (mHost != nullptr) mHost->dispatchDragLeave();
      mXdndSource = 0;
      mXdndTypes.clear();
      mXdndTarget = None;
      mXdndAccepted = false;
      mXdndHovered = false;
    }
  }

  void handleDropSelection(const XSelectionEvent& selection) {
    if (selection.selection != mXdndSelection) return;
    std::string text;
    if (selection.property != None) {
      Atom actualType = None;
      int actualFormat = 0;
      unsigned long count = 0;
      unsigned long remaining = 0;
      unsigned char* data = nullptr;
      if (XGetWindowProperty(mDisplay, mWindow, selection.property, 0, 1u << 20, False,
            AnyPropertyType, &actualType, &actualFormat, &count, &remaining, &data) == Success &&
          data != nullptr) {
        text.assign(reinterpret_cast<const char*>(data), count);
        XFree(data);
      }
      XDeleteProperty(mDisplay, mWindow, selection.property);
    }
    // The XDND target atom name is the clipboard MIME (UTF8_STRING for text/plain), so the same
    // translation the clipboard uses fills the payload the layers see.
    const std::string mime = mimeForSelectionTarget(atomName(mXdndTarget));
    DropEvent event;
    event.position = mDropPosition;
    event.external = true;
    bool accepted = false;
    if (mime == "text/uri-list") {
      event.paths = parseUriList(text);
      accepted = !event.paths.empty();
      if (accepted) event.payload = DragPayload::files(event.paths);
    } else if (mime == "text/plain") {
      accepted = !text.empty();
      if (accepted) event.payload = DragPayload::text(std::move(text));
    } else if (mime == "text/html") {
      accepted = !text.empty();
      if (accepted) event.payload = DragPayload::custom("text/html", std::move(text));
    } else if (mime == "image/png") {
      accepted = !text.empty();
      if (accepted) event.payload = DragPayload::custom("image/png", std::move(text));
    }
    if (accepted && mHost != nullptr) {
      mHost->dispatchDrop(event);
    }
    if (mHost != nullptr) mHost->dispatchDragLeave();
    mXdndHovered = false;
    mXdndAccepted = false;
    finishXdnd(accepted);
    mXdndTarget = None;
    mXdndTypes.clear();
  }

  [[nodiscard]] std::string atomName(const Atom atom) const {
    if (atom == None) return {};
    char* name = XGetAtomName(mDisplay, atom);
    if (name == nullptr) return {};
    std::string result(name);
    XFree(name);
    return result;
  }

  bool createSurface() {
    graphics::gpu::WindowSurface::Configuration configuration;
  configuration.hdrOutput = mOptions.hdrOutput;
    configuration.nativeWindow = reinterpret_cast<void*>(static_cast<uintptr_t>(mWindow));
    configuration.nativeDisplay = mDisplay;
    configuration.nativeWindowSystem = backend::NativeWindowSystem::X11;
    configuration.width = static_cast<uint32_t>(mWidth);
    configuration.height = static_cast<uint32_t>(mHeight);
    configuration.sampleCount = 1;
    configuration.presentMode = backend::PresentMode::Mailbox;
    configuration.preferred = toBackend(mOptions.rendererApi);
    configuration.enableValidation = mOptions.enableValidation;
    mSurface = mSharedDevice != nullptr
      ? graphics::gpu::WindowSurface::makeOn(mSharedDevice, configuration)
      : graphics::gpu::WindowSurface::make(configuration);
    return mSurface != nullptr;
  }

  void handleEvent(XEvent& event) {
    if (mHost == nullptr) return;
    // XIM filters key events that belong to a preedit/commit sequence. Xlib requires the call for
    // every event, not only KeyPress; otherwise the input context's state drifts.
    if (mIme != nullptr && mIme->filterEvent(&event)) return;
    switch (event.type) {
      case ClientMessage:
        if (static_cast<Atom>(event.xclient.data.l[0]) == mDeleteAtom) {
          mHost->requestClose();
        } else {
          handleXdnd(event.xclient);
        }
        break;
      case SelectionNotify:
        handleDropSelection(event.xselection);
        break;
      case PropertyNotify: {
        if (event.xproperty.atom == mNetWmState) readWindowState();
        break;
      }
      case ConfigureNotify: {
        const int width = event.xconfigure.width;
        const int height = event.xconfigure.height;
        if (width > 0 && height > 0 && (width != mWidth || height != mHeight)) {
          mWidth = width;
          mHeight = height;
          if (mSurface != nullptr) {
            mSurface->resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
          }
          mHost->platformResized(width, height);
        }
        break;
      }
      case MotionNotify: {
        PointerEvent pointer;
        pointer.position = { static_cast<float>(event.xmotion.x),
          static_cast<float>(event.xmotion.y) };
        mLastPointer = pointer.position;
        mHost->dispatchPointer(pointer, 0);
        break;
      }
      case LeaveNotify: {
        // The pointer left the window: clear hover instead of leaving the last target highlighted.
        PointerEvent pointer;
        pointer.position = mLastPointer;
        pointer.inside = false;
        mHost->dispatchPointer(pointer, 0);
        break;
      }
      case ButtonPress:
      case ButtonRelease: {
        if (event.xbutton.button >= Button4 && event.xbutton.button <= 7) {
          if (event.type == ButtonPress) {
            ScrollEvent scroll;
            scroll.position = { static_cast<float>(event.xbutton.x),
              static_cast<float>(event.xbutton.y) };
            if (event.xbutton.button == Button4) scroll.deltaY = 48.0f;
            if (event.xbutton.button == Button5) scroll.deltaY = -48.0f;
            if (event.xbutton.button == 6) scroll.deltaX = 48.0f;
            if (event.xbutton.button == 7) scroll.deltaX = -48.0f;
            scroll.timestamp = std::chrono::steady_clock::time_point(
              std::chrono::milliseconds(static_cast<uint64_t>(event.xbutton.time)));
            mHost->dispatchScroll(scroll);
          }
          break;
        }
        if (event.type == ButtonPress && event.xbutton.button == Button1 && mHost != nullptr) {
          // Client-side decorations: let the window-manager perform the interactive move/resize from
          // the caption or border instead of treating the press as a click on the app content.
          const WindowHit hit = mHost->dispatchHitTest({ static_cast<float>(event.xbutton.x),
            static_cast<float>(event.xbutton.y) });
          long direction = -1;
          switch (hit) {
            case WindowHit::Caption: direction = 8; break;
            case WindowHit::ResizeTopLeft: direction = 0; break;
            case WindowHit::ResizeTop: direction = 1; break;
            case WindowHit::ResizeTopRight: direction = 2; break;
            case WindowHit::ResizeRight: direction = 3; break;
            case WindowHit::ResizeBottomRight: direction = 4; break;
            case WindowHit::ResizeBottom: direction = 5; break;
            case WindowHit::ResizeBottomLeft: direction = 6; break;
            case WindowHit::ResizeLeft: direction = 7; break;
            default: break;
          }
          if (direction >= 0) {
            ::Window child = 0;
            int rootX = 0;
            int rootY = 0;
            XTranslateCoordinates(mDisplay, mWindow, DefaultRootWindow(mDisplay),
              event.xbutton.x, event.xbutton.y, &rootX, &rootY, &child);
            XEvent message{};
            message.xclient.type = ClientMessage;
            message.xclient.window = mWindow;
            message.xclient.message_type = XInternAtom(mDisplay, "_NET_WM_MOVERESIZE", False);
            message.xclient.format = 32;
            message.xclient.data.l[0] = rootX;
            message.xclient.data.l[1] = rootY;
            message.xclient.data.l[2] = direction;
            message.xclient.data.l[3] = Button1;
            message.xclient.data.l[4] = 1;
            XSendEvent(mDisplay, DefaultRootWindow(mDisplay), False,
              SubstructureRedirectMask | SubstructureNotifyMask, &message);
            XFlush(mDisplay);
            break;
          }
        }
        PointerEvent pointer;
        pointer.position = { static_cast<float>(event.xbutton.x),
          static_cast<float>(event.xbutton.y) };
        pointer.button = event.xbutton.button == Button1 ? PointerButton::Primary
          : (event.xbutton.button == Button3 ? PointerButton::Secondary : PointerButton::Middle);
        pointer.shift = (event.xbutton.state & ShiftMask) != 0;
        mHost->dispatchPointer(pointer, event.type == ButtonPress ? 1 : 2);
        break;
      }
      case KeyPress:
      case KeyRelease: {
        KeyEvent key;
        char buffer[64]{};
        KeySym symbol = NoSymbol;
        XComposeStatus composeStatus{};
        int lookupStatus = 0;
        int count = 0;
        if (mIme != nullptr) {
          uint32_t imeSymbol = 0;
          count = mIme->lookupString(&event.xkey, buffer, sizeof(buffer), &imeSymbol, &lookupStatus);
          symbol = static_cast<KeySym>(imeSymbol);
        } else {
          count = XLookupString(&event.xkey, buffer, sizeof(buffer), &symbol, &composeStatus);
        }
        key.key = keyFromKeysym(symbol);
        key.shift = (event.xkey.state & ShiftMask) != 0;
        key.control = (event.xkey.state & ControlMask) != 0;
        key.alt = (event.xkey.state & Mod1Mask) != 0;
        mHost->dispatchKey(key, event.type == KeyPress);
        if (event.type == KeyPress && count > 0) {
          // Xutf8LookupString returns UTF-8, not Latin-1: decode the committed sequence into
          // codepoints before handing it to the text input.
          for (int index = 0; index < count;) {
            const auto lead = static_cast<unsigned char>(buffer[index]);
            int length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : 4;
            if (index + length > count) break;
            char32_t codepoint = length == 1 ? lead : (lead & (0x7F >> length));
            for (int part = 1; part < length; ++part) {
              codepoint = (codepoint << 6) |
                (static_cast<unsigned char>(buffer[index + part]) & 0x3F);
            }
            if (codepoint >= 32 && codepoint != 127) mHost->dispatchText(codepoint);
            index += length;
          }
        }
        break;
      }
      case FocusIn:
        if (mIme != nullptr) mIme->focusIn();
        break;
      case FocusOut:
        if (mIme != nullptr) mIme->focusOut();
        break;
      default:
        break;
    }
  }

  Window::Options mOptions;
  backend::Device* mSharedDevice = nullptr;
  std::string mTitle = "Coral UI";
#if defined(CORAL_UI_HAS_ATSPI)
  std::unique_ptr<detail::AtspiBridge> mAccessibility;
#endif
  Display* mDisplay = nullptr;
  std::unique_ptr<ImeBackend> mIme;
  WakePipe mWake;
  ::Window mWindow = 0;
  Atom mDeleteAtom = 0;
  Atom mXdndAware = 0;
  Atom mXdndEnter = 0;
  Atom mXdndPosition = 0;
  Atom mXdndStatus = 0;
  Atom mXdndLeave = 0;
  Atom mXdndDrop = 0;
  Atom mXdndFinished = 0;
  Atom mXdndSelection = 0;
  Atom mXdndActionCopy = 0;
  Atom mUriList = 0;
  Atom mTextHtml = 0;
  Atom mImagePng = 0;
  Atom mUtf8String = 0;
  Atom mDropProperty = 0;
  Atom mNetWmState = 0;
  Atom mMaximizedVert = 0;
  Atom mMaximizedHorz = 0;
  Atom mAbove = 0;
  Atom mHidden = 0;
  Atom mModalAtom = 0;
  Atom mMotifHints = 0;
  Atom mOpacityAtom = 0;
  std::array<Cursor, 9> mCursors{};
  std::string mLastSurroundingText;
  size_t mLastSurroundingCursor = 0;
  size_t mLastSurroundingAnchor = 0;
  /** Targets the current drag source advertised in XdndEnter, in its order. */
  std::vector<Atom> mXdndTypes;
  /** The target the current drop asked for (one type per XDND drop). */
  Atom mXdndTarget = 0;
  /** Whether the layer tree accepted the current position. */
  bool mXdndAccepted = false;
  /** Whether dispatchDragEnter already ran for this drag (the first position dispatches it). */
  bool mXdndHovered = false;
  ::Window mXdndSource = 0;
  Point mDropPosition{};
  std::unique_ptr<graphics::gpu::WindowSurface> mSurface;
  WindowHost* mHost = nullptr;
  int mWidth = 0;
  int mHeight = 0;
  Point mLastPointer{};
  float mDpiScale = 1.0f;
  std::string mDiagnostics;
};

std::unique_ptr<WindowPlatform> createX11Platform(const Window::Options& options,
    backend::Device* sharedDevice, std::string* diagnostics) {
  auto platform = std::make_unique<X11Platform>(options, sharedDevice);
  if (!platform->initialize(diagnostics)) return nullptr;
  return platform;
}

} // namespace coral::ui::detail





