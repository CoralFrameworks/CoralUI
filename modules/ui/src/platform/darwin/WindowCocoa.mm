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
 * @file WindowCocoa.mm
 *
 * The macOS window backend: one standard titled `NSWindow` whose content view is backed by a
 * `CAMetalLayer` and draws the framework's layer tree.
 *
 * The framework owns the event loop (`Window::run`), so this backend never calls `[NSApp run]`. It
 * creates the application/window once, drains AppKit's event queue from `pump()` and blocks in
 * `waitForEvents()` when the UI is idle. Input is translated into the same `PointerEvent`/`KeyEvent`
 * model the Win32 and X11 backends use, in **device pixels**: AppKit speaks points, so every
 * coordinate is multiplied by the backing scale before dispatch, and `WindowImpl` divides it back by
 * `uiScale` for layout.
 *
 * The rendering path is `graphics::gpu::WindowSurface`: the `CAMetalLayer` is handed to the RHI and
 * `present()` queues a drawable. There is no readback or CPU copy in the window path.
 */

#include "../../WindowPlatform.h"

#include <gpu/WindowSurface.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace coral::ui::detail {

class CocoaPlatform;

namespace {

/** AppKit virtual key codes, named so the translation below is readable. */
enum : uint16_t {
  kKeyA = 0, kKeyS = 1, kKeyD = 2, kKeyF = 3, kKeyH = 4, kKeyG = 5, kKeyZ = 6, kKeyX = 7,
  kKeyC = 8, kKeyV = 9, kKeyB = 11, kKeyQ = 12, kKeyW = 13, kKeyE = 14, kKeyR = 15, kKeyY = 16,
  kKeyT = 17, kKey1 = 18, kKey2 = 19, kKey3 = 20, kKey4 = 21, kKey6 = 22, kKey5 = 23,
  kKeyEqual = 24, kKey9 = 25, kKey7 = 26, kKeyMinus = 27, kKey8 = 28, kKey0 = 29,
  kKeyRightBracket = 30, kKeyO = 31, kKeyU = 32, kKeyLeftBracket = 33, kKeyI = 34, kKeyP = 35,
  kKeyReturn = 36, kKeyL = 37, kKeyJ = 38, kKeyQuote = 39, kKeyK = 40, kKeySemicolon = 41,
  kKeyBackslash = 42, kKeyComma = 43, kKeySlash = 44, kKeyN = 45, kKeyM = 46, kKeyPeriod = 47,
  kKeyTab = 48, kKeySpace = 49, kKeyGrave = 50, kKeyDelete = 51, kKeyEscape = 53,
  kKeyCommand = 55, kKeyShift = 56, kKeyCapsLock = 57, kKeyOption = 58, kKeyControl = 59,
  kKeyRightShift = 60, kKeyRightOption = 61, kKeyRightControl = 62, kKeyFunction = 63,
  kKeyF17 = 64, kKeyKeypadDecimal = 65, kKeyKeypadMultiply = 67, kKeyKeypadPlus = 69,
  kKeyKeypadClear = 71, kKeyKeypadDivide = 75, kKeyKeypadEnter = 76, kKeyKeypadMinus = 78,
  kKeyF18 = 79, kKeyF19 = 80, kKeyKeypadEqual = 81, kKeyKeypad0 = 82, kKeyKeypad1 = 83,
  kKeyKeypad2 = 84, kKeyKeypad3 = 85, kKeyKeypad4 = 86, kKeyKeypad5 = 87, kKeyKeypad6 = 88,
  kKeyKeypad7 = 89, kKeyF20 = 90, kKeyKeypad8 = 91, kKeyKeypad9 = 92, kKeyF5 = 96,
  kKeyF6 = 97, kKeyF7 = 98, kKeyF3 = 99, kKeyF8 = 100, kKeyF9 = 101, kKeyF11 = 103,
  kKeyF13 = 105, kKeyF16 = 106, kKeyF14 = 107, kKeyF10 = 109, kKeyF12 = 111, kKeyF15 = 113,
  kKeyHelp = 114, kKeyHome = 115, kKeyPageUp = 116, kKeyForwardDelete = 117, kKeyF4 = 118,
  kKeyEnd = 119, kKeyF2 = 120, kKeyPageDown = 121, kKeyF1 = 122, kKeyLeft = 123,
  kKeyRight = 124, kKeyDown = 125, kKeyUp = 126,
};

Key keyFromCode(const uint16_t code) {
  switch (code) {
    case kKeyA: return Key::A;
    case kKeyB: return Key::B;
    case kKeyC: return Key::C;
    case kKeyD: return Key::D;
    case kKeyE: return Key::E;
    case kKeyF: return Key::F;
    case kKeyG: return Key::G;
    case kKeyH: return Key::H;
    case kKeyI: return Key::I;
    case kKeyJ: return Key::J;
    case kKeyK: return Key::K;
    case kKeyL: return Key::L;
    case kKeyM: return Key::M;
    case kKeyN: return Key::N;
    case kKeyO: return Key::O;
    case kKeyP: return Key::P;
    case kKeyQ: return Key::Q;
    case kKeyR: return Key::R;
    case kKeyS: return Key::S;
    case kKeyT: return Key::T;
    case kKeyU: return Key::U;
    case kKeyV: return Key::V;
    case kKeyW: return Key::W;
    case kKeyX: return Key::X;
    case kKeyY: return Key::Y;
    case kKeyZ: return Key::Z;
    case kKey0: return Key::Digit0;
    case kKey1: return Key::Digit1;
    case kKey2: return Key::Digit2;
    case kKey3: return Key::Digit3;
    case kKey4: return Key::Digit4;
    case kKey5: return Key::Digit5;
    case kKey6: return Key::Digit6;
    case kKey7: return Key::Digit7;
    case kKey8: return Key::Digit8;
    case kKey9: return Key::Digit9;
    case kKeyEscape: return Key::Escape;
    case kKeyReturn: return Key::Enter;
    case kKeyKeypadEnter: return Key::NumpadEnter;
    case kKeySpace: return Key::Space;
    case kKeyTab: return Key::Tab;
    case kKeyDelete: return Key::Backspace;
    case kKeyForwardDelete: return Key::Delete;
    case kKeyLeft: return Key::Left;
    case kKeyRight: return Key::Right;
    case kKeyUp: return Key::Up;
    case kKeyDown: return Key::Down;
    case kKeyHome: return Key::Home;
    case kKeyEnd: return Key::End;
    case kKeyPageUp: return Key::PageUp;
    case kKeyPageDown: return Key::PageDown;
    case kKeyHelp: return Key::Insert;
    case kKeyComma: return Key::Comma;
    case kKeyPeriod: return Key::Period;
    case kKeySlash: return Key::Slash;
    case kKeyBackslash: return Key::Backslash;
    case kKeySemicolon: return Key::Semicolon;
    case kKeyQuote: return Key::Apostrophe;
    case kKeyLeftBracket: return Key::LeftBracket;
    case kKeyRightBracket: return Key::RightBracket;
    case kKeyMinus: return Key::Minus;
    case kKeyEqual: return Key::Equal;
    case kKeyGrave: return Key::Grave;
    case kKeyShift: return Key::LeftShift;
    case kKeyRightShift: return Key::RightShift;
    case kKeyControl: return Key::LeftControl;
    case kKeyRightControl: return Key::RightControl;
    case kKeyOption: return Key::LeftAlt;
    case kKeyRightOption: return Key::RightAlt;
    case kKeyCommand: return Key::LeftMeta;
    case kKeyFunction: return Key::Unknown;
    case kKeyCapsLock: return Key::CapsLock;
    case kKeyF1: return Key::F1;
    case kKeyF2: return Key::F2;
    case kKeyF3: return Key::F3;
    case kKeyF4: return Key::F4;
    case kKeyF5: return Key::F5;
    case kKeyF6: return Key::F6;
    case kKeyF7: return Key::F7;
    case kKeyF8: return Key::F8;
    case kKeyF9: return Key::F9;
    case kKeyF10: return Key::F10;
    case kKeyF11: return Key::F11;
    case kKeyF12: return Key::F12;
    case kKeyKeypad0: return Key::Numpad0;
    case kKeyKeypad1: return Key::Numpad1;
    case kKeyKeypad2: return Key::Numpad2;
    case kKeyKeypad3: return Key::Numpad3;
    case kKeyKeypad4: return Key::Numpad4;
    case kKeyKeypad5: return Key::Numpad5;
    case kKeyKeypad6: return Key::Numpad6;
    case kKeyKeypad7: return Key::Numpad7;
    case kKeyKeypad8: return Key::Numpad8;
    case kKeyKeypad9: return Key::Numpad9;
    case kKeyKeypadMultiply: return Key::NumpadMultiply;
    case kKeyKeypadPlus: return Key::NumpadAdd;
    case kKeyKeypadMinus: return Key::NumpadSubtract;
    case kKeyKeypadDecimal: return Key::NumpadDecimal;
    case kKeyKeypadDivide: return Key::NumpadDivide;
    case kKeyKeypadEqual: return Key::NumpadEqual;
    case kKeyKeypadClear: return Key::NumLock;
    default: break;
  }
  return Key::Unknown;
}

bool isModifierKey(const Key key) {
  switch (key) {
    case Key::LeftShift:
    case Key::RightShift:
    case Key::LeftControl:
    case Key::RightControl:
    case Key::LeftAlt:
    case Key::RightAlt:
    case Key::LeftMeta:
    case Key::RightMeta:
    case Key::CapsLock:
      return true;
    default:
      return false;
  }
}

/** Modifier state for one `Key` from an AppKit flag mask. */
bool modifierIsDown(const Key key, const NSEventModifierFlags flags) {
  switch (key) {
    case Key::LeftShift:
    case Key::RightShift:
      return (flags & NSEventModifierFlagShift) != 0;
    case Key::LeftControl:
    case Key::RightControl:
      return (flags & NSEventModifierFlagControl) != 0;
    case Key::LeftAlt:
    case Key::RightAlt:
      return (flags & NSEventModifierFlagOption) != 0;
    case Key::LeftMeta:
    case Key::RightMeta:
      return (flags & NSEventModifierFlagCommand) != 0;
    case Key::CapsLock:
      return (flags & NSEventModifierFlagCapsLock) != 0;
    default:
      return false;
  }
}

uint32_t argbFromColor(NSColor* color) {
  if (color == nil) return 0;
  NSColor* rgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
  if (rgb == nil) return 0;
  const CGFloat red = std::clamp(rgb.redComponent, 0.0, 1.0);
  const CGFloat green = std::clamp(rgb.greenComponent, 0.0, 1.0);
  const CGFloat blue = std::clamp(rgb.blueComponent, 0.0, 1.0);
  const CGFloat alpha = std::clamp(rgb.alphaComponent, 0.0, 1.0);
  const auto channel = [](const CGFloat value) {
    return static_cast<uint32_t>(std::lround(value * 255.0));
  };
  return (channel(alpha) << 24) | (channel(red) << 16) | (channel(green) << 8) | channel(blue);
}

} // namespace

} // namespace coral::ui::detail

/** The content view: first responder, pointer tracking and AppKit event translation. */
@interface CoralUIView : NSView
@property(nonatomic, assign) void* platform;
@property(nonatomic, strong) NSTrackingArea* coralTrackingArea;
@end

/** A borderless window still has to accept key/main status, or client-decorated windows lose input. */
@interface CoralWindow : NSWindow
@end

/** Process-wide AppKit delegate. Window delegates stay per-window; this one owns the application. */
@interface CoralAppDelegate : NSObject <NSApplicationDelegate>
@end

/** The window delegate: resize/DPI/appearance callbacks and close interception. */
@interface CoralUIWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) void* platform;
@end

namespace coral::ui::detail {

/**
 * The macOS `WindowPlatform`.
 *
 * One instance owns one `NSWindow`, one content view and exactly one `gpu::WindowSurface`. With
 * `RenderHostKind::RenderThread` the surface is created and presented by CoralUI's render thread, so
 * this object only publishes the layer handle and the logical size in that mode.
 */
class CocoaPlatform final : public WindowPlatform {
 public:
  CocoaPlatform(Window::Options options, backend::Device* sharedDevice)
    : mOptions(std::move(options)), mSharedDevice(sharedDevice),
      mWidth(std::max(1, mOptions.width)), mHeight(std::max(1, mOptions.height)) {}

  ~CocoaPlatform() override {
    mSurface.reset();
    mDeviceOwner.reset();
    mHost = nullptr;
    if (mWindow != nil) {
      mWindow.delegate = nil;
      [mWindow orderOut:nil];
      [mWindow close];
      mWindow = nil;
    }
    mView = nil;
    mDelegate = nil;
  }

  bool initialize(std::string* diagnostics) {
    if (![NSThread isMainThread]) {
      if (diagnostics != nullptr) {
        *diagnostics = "the Cocoa window backend has to be created on the main thread";
      }
      return false;
    }
    if (!backend::isBackendAvailable(backend::Backend::Metal)) {
      if (diagnostics != nullptr) {
        *diagnostics = "this build has no Metal backend, which the Cocoa window backend presents through";
      }
      return false;
    }
    if (mOptions.rendererApi == RendererApi::OpenGL ||
        mOptions.rendererApi == RendererApi::Vulkan) {
      if (diagnostics != nullptr) {
        *diagnostics = "the Cocoa window backend currently presents through Metal only";
      }
      return false;
    }

    installApplication();

    NSScreen* screen = [NSScreen mainScreen];
    mBackingScale = screen != nil ? static_cast<float>(screen.backingScaleFactor) : 1.0f;
    if (mBackingScale <= 0.0f) mBackingScale = 1.0f;
    mDpiScale = mBackingScale;

    // `Window::Options::width`/`height` are the client size. macOS window geometry is expressed in
    // points, so a 960x540 request opens a 960x540-point window and reports 1920x1080 backing pixels
    // on a 2x display; `WindowImpl` then lays the tree out at 960x540 logical units. This is what the
    // other backends get at 100% DPI, and what makes a Retina window the same apparent size as a
    // Windows one at the same setting.
    const NSSize contentPoints = NSMakeSize(static_cast<CGFloat>(mWidth),
      static_cast<CGFloat>(mHeight));
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    mWindow = [[CoralWindow alloc] initWithContentRect:NSMakeRect(0, 0, contentPoints.width,
                                                          contentPoints.height)
                                             styleMask:style
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
    mWindow.hasShadow = YES;
    if (mWindow == nil) {
      if (diagnostics != nullptr) *diagnostics = "the Cocoa window could not be created";
      return false;
    }
    mWindow.releasedWhenClosed = NO;
    mWindow.title = [NSString stringWithUTF8String:mOptions.title.c_str()];
    if (mOptions.position.has_value()) {
      const NSScreen* targetScreen = mWindow.screen != nil ? mWindow.screen : screen;
      const CGFloat screenHeight = targetScreen != nil ? targetScreen.frame.size.height : 0.0;
      const CGFloat top = static_cast<CGFloat>(mOptions.position->y);
      const CGFloat height = mWindow.frame.size.height;
      [mWindow setFrameOrigin:NSMakePoint(static_cast<CGFloat>(mOptions.position->x),
        std::max<CGFloat>(0.0, screenHeight - top - height))];
    } else {
      [mWindow center];
    }

    mView = [[CoralUIView alloc] initWithFrame:NSMakeRect(0, 0, contentPoints.width,
                                                   contentPoints.height)];
    mView.platform = this;
    mView.wantsLayer = YES;
    if (![mView.layer isKindOfClass:[CAMetalLayer class]]) {
      if (diagnostics != nullptr) *diagnostics = "the Cocoa content view has no CAMetalLayer";
      return false;
    }
    mMetalLayer = (CAMetalLayer*) mView.layer;
    mMetalLayer.contentsScale = mWindow != nil ? mWindow.backingScaleFactor : mDpiScale;
    mMetalLayer.opaque = YES;
    // AppKit resizes the view before the next drawable exists. The default `resize` gravity scales
    // the previous frame to the new bounds, which is what made a live resize look like the page was
    // stretching; pinning the old frame to the top-left leaves the new strip blank until the frame
    // we render from the resize callback fills it.
    mMetalLayer.contentsGravity = kCAGravityTopLeft;
    // NSView owns the layer's bounds/position, but an implicit Core Animation action on either one
    // makes the content chase the window with a short animation during a live resize -- visible as a
    // vertical bounce because the height changes. The frame must be immediate.
    mMetalLayer.actions = @{
      @"bounds": [NSNull null],
      @"position": [NSNull null],
      @"contents": [NSNull null],
      @"drawableSize": [NSNull null]
    };
    if (std::getenv("CORAL_UI_COCOA_DEBUG") != nullptr) {
      std::fprintf(stderr, "[cocoa] view flipped=%d layer geometryFlipped=%d gravity=%s\n",
        mView.isFlipped ? 1 : 0, mMetalLayer.geometryFlipped ? 1 : 0,
        mMetalLayer.contentsGravity.UTF8String ?: "nil");
    }

    mDelegate = [[CoralUIWindowDelegate alloc] init];
    mDelegate.platform = this;
    mWindow.delegate = mDelegate;
    mWindow.contentView = mView;
    [mWindow makeFirstResponder:mView];
    [mWindow makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    updateBackingSize(false);

    if (mOptions.renderHost == RenderHostKind::RenderThread) {
      // The render thread owns the surface; `nativeWindow()` hands it this layer.
      return true;
    }
    if (!createSurface()) {
      if (diagnostics != nullptr) {
        *diagnostics = mDiagnostics.empty()
          ? "the Metal window surface could not be created" : mDiagnostics;
      }
      return false;
    }
    return true;
  }

  [[nodiscard]] Window::NativePlatform kind() const override {
    return Window::NativePlatform::Cocoa;
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
    mHost = &host;
    bool alive = true;
    @autoreleasepool {
      for (;;) {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event == nil) break;
        [NSApp sendEvent:event];
        if (mClosing) {
          alive = false;
          break;
        }
      }
    }
    return alive && mWindow != nil;
  }

  void waitForEvents(const int timeoutMs) override {
    if (mWindow == nil || timeoutMs == 0) return;
    @autoreleasepool {
      NSDate* until = timeoutMs < 0
        ? [NSDate distantFuture]
        : [NSDate dateWithTimeIntervalSinceNow:static_cast<NSTimeInterval>(timeoutMs) / 1000.0];
      NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                          untilDate:until
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event != nil) {
        [NSApp sendEvent:event];
      }
    }
  }

  void wake() override {
    // `requestFrame` can be called from a worker thread (a completed task posts one). `postEvent:`
    // is documented as thread-safe, and the application-defined event makes a blocked
    // `nextEventMatchingMask:` return; `CFRunLoopWakeUp` covers a wait that has not entered AppKit's
    // queue yet.
    postWakeEvent();
    CFRunLoopWakeUp(CFRunLoopGetMain());
  }

  [[nodiscard]] graphics::Canvas* beginFrame() override {
    if (mSurface == nullptr) return nullptr;
    const auto resizeStart = std::chrono::steady_clock::now();
    if (mSurface->width() != static_cast<uint32_t>(mWidth) ||
        mSurface->height() != static_cast<uint32_t>(mHeight)) {
      if (!mSurface->resize(static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight))) {
        mDiagnostics = mSurface->diagnostics();
        return nullptr;
      }
    }
    const auto acquireStart = std::chrono::steady_clock::now();
    graphics::Canvas* canvas = mSurface->beginFrame();
    const auto end = std::chrono::steady_clock::now();
    mLastResizeMilliseconds = std::chrono::duration<double, std::milli>(acquireStart - resizeStart).count();
    mLastAcquireMilliseconds = std::chrono::duration<double, std::milli>(end - acquireStart).count();
    return canvas;
  }

  bool present() override {
    if (mSurface == nullptr) return false;
    const auto start = std::chrono::steady_clock::now();
    const bool ok = mSurface->present();
    mLastPresentMilliseconds = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count();
    if (!ok) mDiagnostics = mSurface->diagnostics();
    return ok;
  }

  [[nodiscard]] bool snapshot(graphics::Bitmap* out) override {
    return mSurface != nullptr && mSurface->readback(out);
  }

  bool resize(uint32_t width, uint32_t height) override {
    if (width == 0 || height == 0) return false;
    mWidth = static_cast<int>(width);
    mHeight = static_cast<int>(height);
    if (mWindow == nil) {
      mDiagnostics = "the Cocoa window does not exist";
      return false;
    }
    // `Window::resize` speaks device pixels; AppKit sizes in points. This must use the display's
    // backing scale, not `mDpiScale`: during a live resize the latter also carries the half-resolution
    // render scale, and a programmatic resize arriving then would size the window twice as large.
    const NSSize points = NSMakeSize(static_cast<CGFloat>(width) / mBackingScale,
      static_cast<CGFloat>(height) / mBackingScale);
    mAdjustingContentSize = true;
    [mWindow setContentSize:points];
    mAdjustingContentSize = false;
    updateBackingSize(true);
    if (mOptions.renderHost == RenderHostKind::RenderThread) {
      return true;
    }
    if (mSurface == nullptr) {
      return createSurface();
    }
    if (!mSurface->resize(static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight))) {
      mDiagnostics = mSurface->diagnostics();
      return false;
    }
    return true;
  }

  bool recreate() override {
    if (mOptions.renderHost == RenderHostKind::RenderThread) {
      return mWindow != nil;
    }
    mSurface.reset();
    return createSurface();
  }

  void setTitle(std::string_view title) override {
    if (mWindow == nil) return;
    mWindow.title = [NSString stringWithUTF8String:std::string(title).c_str()];
  }

  void setPosition(const int x, const int y) override {
    if (mWindow == nil) return;
    NSScreen* screen = mWindow.screen != nil ? mWindow.screen : [NSScreen mainScreen];
    const CGFloat screenHeight = screen != nil ? screen.frame.size.height : 0.0;
    // The public API uses top-left screen coordinates; AppKit's origin is bottom-left.
    const CGFloat height = mWindow.frame.size.height;
    [mWindow setFrameOrigin:NSMakePoint(static_cast<CGFloat>(x),
      screenHeight - static_cast<CGFloat>(y) - height)];
  }

  void setMinimumSize(const int width, const int height) override {
    if (mWindow == nil) return;
    mWindow.contentMinSize = NSMakeSize(
      width > 0 ? static_cast<CGFloat>(width) / mBackingScale : 0.0,
      height > 0 ? static_cast<CGFloat>(height) / mBackingScale : 0.0);
  }

  void setMaximized(const bool maximized) override {
    if (mWindow == nil) return;
    // The window is always a standard titled window on macOS, so AppKit owns zoom and the resize
    // frame. That is what keeps dragging and edge resizing on the window server's own path.
    const bool zoomed = [mWindow isZoomed] != NO;
    if (maximized && !zoomed) {
      [mWindow zoom:nil];
    } else if (!maximized && zoomed) {
      [mWindow zoom:nil];
    }
    mMaximized = [mWindow isZoomed] != NO;
    if (mHost != nullptr) {
      mHost->platformStateChanged(mMaximized, mMinimized, mFullscreen);
    }
  }
  void setMinimized(const bool minimized) override {
    if (mWindow == nil) return;
    if (minimized) {
      [mWindow miniaturize:nil];
    } else {
      [mWindow deminiaturize:nil];
    }
    mMinimized = minimized;
    if (mHost != nullptr) {
      mHost->platformStateChanged(mMaximized, mMinimized, mFullscreen);
    }
  }

  void setFullscreen(const bool fullscreen) override {
    if (mWindow == nil) return;
    const bool isFullscreen = (mWindow.styleMask & NSWindowStyleMaskFullScreen) != 0;
    if (fullscreen != isFullscreen) {
      [mWindow toggleFullScreen:nil];
    }
  }

  void setCursor(Window::Cursor cursor) override {
    mCursor = cursor;
    NSCursor* native = nil;
    switch (cursor) {
      case Window::Cursor::IBeam: native = [NSCursor IBeamCursor]; break;
      case Window::Cursor::Hand: native = [NSCursor pointingHandCursor]; break;
      case Window::Cursor::ResizeHorizontal: native = [NSCursor resizeLeftRightCursor]; break;
      case Window::Cursor::ResizeVertical: native = [NSCursor resizeUpDownCursor]; break;
      case Window::Cursor::Move: native = [NSCursor openHandCursor]; break;
      case Window::Cursor::Crosshair: native = [NSCursor crosshairCursor]; break;
      case Window::Cursor::NotAllowed: native = [NSCursor operationNotAllowedCursor]; break;
      case Window::Cursor::Wait: native = [NSCursor arrowCursor]; break;
      case Window::Cursor::Arrow:
      default: native = [NSCursor arrowCursor]; break;
    }
    mNativeCursor = native;
    [native set];
  }

  void setPointerCapture(const bool captured) override {
    // AppKit delivers drags to the view that received the press for as long as the button is down,
    // which is the implicit capture a widget needs.
    (void)captured;
  }

  void setDecorations(Window::Decorations decorations) override {
    (void)decorations;
    if (mWindow == nil) return;
    // ## macOS always uses system chrome
    //
    // The window server owns the drag, live resize, zoom, Spaces and the edge hit testing of a
    // standard titled NSWindow. A borderless window that draws its own caption has to reimplement
    // those paths and never quite matches them: edges become hard to grab and the window can stop
    // following the pointer. CoralUI therefore keeps the standard frame here and puts the custom
    // layer tree in the content view. `Decorations::Client` is accepted for API compatibility but
    // maps to the system frame on this backend.
    NSWindowStyleMask desired = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    if (mWindow.styleMask != desired) {
      mWindow.styleMask = desired;
    }
    mWindow.titleVisibility = NSWindowTitleVisible;
    mWindow.titlebarAppearsTransparent = NO;
    for (NSWindowButton button: { NSWindowCloseButton, NSWindowMiniaturizeButton,
             NSWindowZoomButton }) {
      [mWindow standardWindowButton:button].hidden = NO;
    }
  }
  void setOpacity(float opacity) override {
    if (mWindow != nil) mWindow.alphaValue = std::clamp(static_cast<CGFloat>(opacity), 0.0, 1.0);
  }

  void setAlwaysOnTop(bool onTop) override {
    if (mWindow != nil) {
      mWindow.level = onTop ? NSFloatingWindowLevel : NSNormalWindowLevel;
    }
  }

  void setTransientFor(WindowPlatform* parent) override {
    if (mWindow == nil) return;
    auto* cocoa = dynamic_cast<CocoaPlatform*>(parent);
    if (cocoa != nullptr && cocoa->mWindow != nil) {
      [cocoa->mWindow addChildWindow:mWindow ordered:NSWindowAbove];
    }
  }

  void setModal(bool modal) override {
    (void)modal;
  }

  [[nodiscard]] bool prefersDarkAppearance() const override {
    return systemAppearance().preferDark;
  }

  [[nodiscard]] SystemAppearance systemAppearance() const override {
    SystemAppearance appearance;
    if (mWindow == nil) return appearance;
    appearance.valid = true;
    NSAppearance* effective = mWindow.effectiveAppearance;
    if (@available(macOS 10.14, *)) {
      NSAppearanceName best = [effective bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua
      ]];
      appearance.preferDark = [best isEqualToString:NSAppearanceNameDarkAqua];
    } else {
      appearance.preferDark = NO;
    }
    NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
    if (@available(macOS 10.12, *)) {
      appearance.reduceMotion = workspace.accessibilityDisplayShouldReduceMotion;
      appearance.highContrast = workspace.accessibilityDisplayShouldIncreaseContrast;
    }
    if (appearance.highContrast) {
      SystemColors& colors = appearance.colors;
      colors.valid = true;
      colors.window = argbFromColor([NSColor windowBackgroundColor]);
      colors.windowText = argbFromColor([NSColor labelColor]);
      colors.buttonFace = argbFromColor([NSColor controlBackgroundColor]);
      colors.buttonText = argbFromColor([NSColor controlTextColor]);
      colors.highlight = argbFromColor([NSColor selectedContentBackgroundColor]);
      colors.highlightText = argbFromColor([NSColor selectedMenuItemTextColor]);
      colors.grayText = argbFromColor([NSColor secondaryLabelColor]);
    }
    return appearance;
  }

  void raise() override {
    if (mWindow == nil) return;
    [mWindow makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
  }

  [[nodiscard]] Rect screenBounds() const override {
    if (mWindow == nil) return Rect{};
    const NSRect frame = mWindow.frame;
    const NSScreen* screen = mWindow.screen != nil ? mWindow.screen : [NSScreen mainScreen];
    const CGFloat screenHeight = screen != nil ? screen.frame.size.height : 0.0;
    return Rect::MakeLTRB(static_cast<float>(frame.origin.x),
      static_cast<float>(screenHeight - frame.origin.y - frame.size.height),
      static_cast<float>(frame.origin.x + frame.size.width),
      static_cast<float>(screenHeight - frame.origin.y));
  }

  [[nodiscard]] std::string diagnostics() const override {
    return mDiagnostics;
  }

  [[nodiscard]] graphics::gpu::NativeDisplay nativeDisplay() const override {
    // AppKit has no presentation-feedback source here, so the window uses its present clock. The
    // RHI still needs the native handle for the render-thread path, which travels through
    // `nativeWindow()` instead.
    return {};
  }

  [[nodiscard]] void* nativeWindow() const override {
    return mUsingMetal ? (__bridge void*) mMetalLayer : (__bridge void*) mView;
  }

  [[nodiscard]] backend::Device* device() const override {
    return mSurface != nullptr ? mSurface->device() : mDeviceOwner.get();
  }

  [[nodiscard]] backend::Device* releaseDevice() override {
    if (mDeviceOwner != nullptr) {
      backend::Device* device = mDeviceOwner.release();
      mSharedDevice = device;
      return device;
    }
    return mSurface != nullptr ? mSurface->releaseDevice() : nullptr;
  }

  [[nodiscard]] uint32_t sampleCount() const override {
    return mSurface != nullptr ? mSurface->sampleCount() : 1;
  }

  [[nodiscard]] FrameRenderTiming lastFrameRenderTiming() const override {
    if (mSurface == nullptr) return {};
    const graphics::gpu::FrameStages stages = mSurface->lastFrameStages();
    return FrameRenderTiming{ stages.record, stages.execute, mSurface->lastFrameGpuMilliseconds(),
      stages.total, mSurface->residentBytes() };
  }

  [[nodiscard]] bool outputIsHdr() const override {
    return false;
  }

  [[nodiscard]] uint32_t lastFrameCommandBuffers() const override {
    return mSurface != nullptr ? mSurface->lastFrameSubmitCount() : 0;
  }
  [[nodiscard]] graphics::gpu::FrameStages lastFrameStages() const override {
    return mSurface != nullptr ? mSurface->lastFrameStages() : graphics::gpu::FrameStages{};
  }
  [[nodiscard]] uint32_t lastFrameDrawCalls() const override {
    return mSurface != nullptr ? mSurface->lastFrameDrawCallCount() : 0;
  }
  [[nodiscard]] uint32_t lastFrameBatches() const override {
    return mSurface != nullptr ? mSurface->lastFrameBatchCount() : 0;
  }

  // --- AppKit callbacks (called from the view/delegate) ----------------------------------------

  /**
   * AppKit's content view is bottom-left even though the UI's layer tree is top-left. Keeping the
   * view unflipped means its backing CAMetalLayer is not geometry-flipped either, so
   * `contentsGravity = topLeft` really is the visual top-left while the window changes height; the
   * flipped view made vertical resizes place the old drawable from the other edge and jitter. The
   * one place that has to flip is input: convert the point here.
   */
  Point devicePointForEvent(NSEvent* event) const {
    const NSPoint point = [mView convertPoint:event.locationInWindow fromView:nil];
    const CGFloat y = mView.bounds.size.height - point.y;
    return Point{ static_cast<float>(point.x) * mDpiScale, static_cast<float>(y) * mDpiScale };
  }

  void dispatchPointerEvent(NSEvent* event, const PointerButton button, const int phase,
      const bool inside) {
    if (mHost == nullptr) return;
    // A live resize owns the main thread's frame budget. Hover/move events would each call
    // `markFrameNeeded()` and make the normal on-demand loop compete with the resize frames; the
    // resize frames already redraw what the user is looking at, so input routing can wait until the
    // drag ends.
    if (mWindow != nil && mWindow.inLiveResize) return;
    const Point device = devicePointForEvent(event);
    PointerEvent pointer;
    pointer.position = { device.x, device.y };
    pointer.button = button;
    pointer.inside = inside;
    pointer.shift = (event.modifierFlags & NSEventModifierFlagShift) != 0;
    pointer.kind = PointerKind::Mouse;
    pointer.pointerId = 0;
    pointer.pressure = phase == 2 ? 0.0f : 1.0f;
    if (std::getenv("CORAL_UI_COCOA_DEBUG") != nullptr) {
      std::fprintf(stderr, "[cocoa] pointer phase=%d device=%.1f,%.1f inside=%d button=%d\n", phase,
        static_cast<double>(pointer.position.x), static_cast<double>(pointer.position.y),
        inside ? 1 : 0, static_cast<int>(button));
    }
    mHost->dispatchPointer(pointer, phase);
    if (mNativeCursor != nil) [mNativeCursor set];
  }

  void dispatchScrollEvent(NSEvent* event) {
    if (mHost == nullptr) return;
    if (mWindow != nil && mWindow.inLiveResize) return;
    const Point device = devicePointForEvent(event);
    ScrollEvent scroll;
    scroll.position = { device.x, device.y };
    CGFloat deltaX = event.scrollingDeltaX;
    CGFloat deltaY = event.scrollingDeltaY;
    if (!event.hasPreciseScrollingDeltas) {
      // A line-based wheel reports a small integer; the framework's scroll positions are in device
      // pixels, so promote it to the same scale a precise trackpad delta uses.
      deltaX *= 48.0;
      deltaY *= 48.0;
    }
    scroll.deltaX = static_cast<float>(deltaX) * mDpiScale;
    scroll.deltaY = static_cast<float>(deltaY) * mDpiScale;
    mHost->dispatchScroll(scroll);
  }

  KeyEvent keyEvent(NSEvent* event) const {
    KeyEvent result;
    result.key = keyFromCode(event.keyCode);
    result.repeat = event.isARepeat != NO;
    const NSEventModifierFlags flags = event.modifierFlags;
    result.shift = (flags & NSEventModifierFlagShift) != 0;
    result.control = (flags & NSEventModifierFlagControl) != 0;
    result.alt = (flags & NSEventModifierFlagOption) != 0;
    result.meta = (flags & NSEventModifierFlagCommand) != 0;

    NSString* characters = event.characters;
    if (characters.length > 0) {
      const unichar first = [characters characterAtIndex:0];
      if (first >= 0x20 && first != 0x7F) result.character = first;
    }
    return result;
  }

  void dispatchKeyEvent(NSEvent* event, const bool down) {
    if (mHost == nullptr) return;
    const KeyEvent translated = keyEvent(event);
    if (translated.key == Key::Unknown) return;
    mHost->dispatchKey(translated, down);
  }

  void dispatchTextEvent(NSEvent* event) {
    if (mHost == nullptr) return;
    const KeyEvent translated = keyEvent(event);
    // Command and Control chords are shortcuts, not text. Option is a text modifier (it produces
    // characters such as "å"), so it is allowed to reach the focused input.
    if (translated.control || translated.meta) return;
    NSString* characters = event.characters;
    if (characters.length == 0) return;
    for (NSUInteger index = 0; index < characters.length; ++index) {
      const unichar first = [characters characterAtIndex:index];
      uint32_t scalar = first;
      if (CFStringIsSurrogateHighCharacter(first) && index + 1 < characters.length) {
        const unichar second = [characters characterAtIndex:index + 1];
        if (CFStringIsSurrogateLowCharacter(second)) {
          scalar = CFStringGetLongCharacterForSurrogatePair(first, second);
          ++index;
        }
      }
      if (scalar < 0x20 || scalar == 0x7F) continue;
      mHost->dispatchText(static_cast<char32_t>(scalar));
    }
  }

  void dispatchModifierEvent(NSEvent* event) {
    if (mHost == nullptr) return;
    const Key key = keyFromCode(event.keyCode);
    if (!isModifierKey(key)) return;
    const bool down = modifierIsDown(key, event.modifierFlags);
    KeyEvent translated = keyEvent(event);
    translated.key = key;
    mHost->dispatchKey(translated, down);
  }

  /** Called by the window delegate when AppKit changed the content size or backing scale. */
  void windowDidChangeGeometry() {
    if (mWindow == nil) return;
    const bool changed = updateBackingSize(true);
    if (!changed || mHost == nullptr || mWindow == nil || !mWindow.inLiveResize) return;
    // Render the size the user is looking at (including the very first resize event), but pace the
    // next one from what this frame actually cost: a 16 ms frame at 60 Hz would otherwise leave the
    // main thread no time to process AppKit's resize events, which is what a 15-18 ms plateau in
    // the debug log looks like. A cheap frame is allowed at 60 Hz; an expensive one backs off to
    // ~30 Hz until the tree gets cheaper again.
    const auto now = std::chrono::steady_clock::now();
    if (mHasInteractiveRender && now < mNextInteractiveRender) {
      return;
    }
    mHasInteractiveRender = true;
    const auto renderStart = now;
    mHost->renderFrameNow();
    const auto cost = std::chrono::steady_clock::now() - renderStart;
    mLastInteractiveCost = cost;
    auto interval = cost * 2;
    if (interval < std::chrono::milliseconds(16)) interval = std::chrono::milliseconds(16);
    if (interval > std::chrono::milliseconds(50)) interval = std::chrono::milliseconds(50);
    mNextInteractiveRender = renderStart + interval;
    if (std::getenv("CORAL_UI_COCOA_DEBUG") != nullptr) {
      const graphics::gpu::FrameStages stages = mSurface != nullptr
        ? mSurface->lastFrameStages() : graphics::gpu::FrameStages{};
      std::fprintf(stderr,
        "[cocoa] live resize %dx%d frame=%.2fms next=%.1fms resize=%.2f acquire=%.2f "
        "present=%.2f stages(fields=%.2f targets=%.2f record=%.2f execute=%.2f total=%.2f)\n",
        mWidth, mHeight, std::chrono::duration<double, std::milli>(cost).count(),
        std::chrono::duration<double, std::milli>(interval).count(), mLastResizeMilliseconds,
        mLastAcquireMilliseconds, mLastPresentMilliseconds, stages.fields, stages.targets,
        stages.record, stages.execute, stages.total);
    }
  }

  void windowWillStartLiveResize() {
    // Nothing on the layer is changed at the start of a resize: the system frame owns the drag, and
    // switching presentation or resolution here made the first frame flash. Only the interactive
    // throttle is reset.
    mHasInteractiveRender = false;
    mNextInteractiveRender = {};
    mLastInteractiveCost = {};
  }

  void windowDidEndLiveResize() {
    mHasInteractiveRender = false;
    mNextInteractiveRender = {};
    mLastInteractiveCost = {};
    if (mHost != nullptr) {
      // One final full-resolution frame so the content lands exactly where the window stopped.
      mHost->renderFrameNow();
    }
  }

  void windowVisibilityChanged(const bool visible) {
    if (mHost == nullptr) return;
    mHost->platformStateChanged(mMaximized, mMinimized, mFullscreen);
    if (visible) mHost->requestFrame();
  }

  void windowStateChanged() {
    if (mWindow == nil) return;
    mMaximized = [mWindow isZoomed] != NO;
    mMinimized = [mWindow isMiniaturized] != NO;
    mFullscreen = (mWindow.styleMask & NSWindowStyleMaskFullScreen) != 0;
    if (mHost != nullptr) {
      mHost->platformStateChanged(mMaximized, mMinimized, mFullscreen);
    }
    updateBackingSize(true);
  }

  void windowShouldCloseRequested() {
    mClosing = true;
    if (mHost != nullptr) {
      mHost->requestClose();
    }
  }

  void windowAppearanceChanged() {
    if (mHost == nullptr) return;
    mHost->systemAppearanceChanged();
    mHost->requestFrame();
  }

  void windowDidChangeScreen() {
    if (mWindow == nil) return;
    updateBackingSize(true);
  }

  // --- Responder-chain entry points used by the native menus ----------------------------------
  /**
   * Runs one of the standard editing commands through the same key path the controls use.
   *
   * A native Edit menu sends an action down the responder chain rather than a key event; the focused
   * CoralUI layer only knows `KeyEvent`, so the menu action is translated back into the command chord
   * that `TextInput` already handles.
   */
  void performEditCommand(const Key key, const bool shift = false, const bool meta = true) {
    if (mHost == nullptr) return;
    KeyEvent event;
    event.key = key;
    event.shift = shift;
    event.meta = meta;
    mHost->dispatchKey(event, true);
  }

  [[nodiscard]] bool hasTextFocus() const {
    if (mHost == nullptr) return false;
    std::string text;
    size_t cursor = 0;
    size_t anchor = 0;
    return mHost->surroundingText(&text, &cursor, &anchor);
  }

  void requestCloseFromMenu() {
    if (mHost != nullptr) mHost->requestClose();
  }

 private:
  bool createSurface() {
    if (mView == nil || mMetalLayer == nil) {
      mDiagnostics = "the Cocoa content view is not ready";
      return false;
    }
    graphics::gpu::WindowSurface::Configuration configuration;
    configuration.nativeWindow = (__bridge void*) mMetalLayer;
    configuration.nativeWindowSystem = backend::NativeWindowSystem::Metal;
    configuration.width = static_cast<uint32_t>(mWidth);
    configuration.height = static_cast<uint32_t>(mHeight);
    configuration.sampleCount = 1;
    // Mailbox matches the Win32 window: the compositor keeps the newest queued frame instead of
    // making `nextDrawable` wait for the previous present.
    configuration.presentMode = backend::PresentMode::Mailbox;
    configuration.preferred = mOptions.rendererApi == RendererApi::Auto
      ? backend::Backend::Null : toBackend(mOptions.rendererApi);
    configuration.enableValidation = mOptions.enableValidation;

    backend::Device* device = mSharedDevice != nullptr ? mSharedDevice : mDeviceOwner.get();
    mSurface = device != nullptr
      ? graphics::gpu::WindowSurface::makeOn(device, configuration)
      : graphics::gpu::WindowSurface::make(configuration);
    if (mSurface == nullptr) {
      mDiagnostics = "the Metal window surface could not be created for the CAMetalLayer";
      return false;
    }
    if (device == nullptr) {
      mDeviceOwner.reset(mSurface->releaseDevice());
      if (mDeviceOwner != nullptr) {
        mSurface = graphics::gpu::WindowSurface::makeOn(mDeviceOwner.get(), configuration);
      }
    }
    if (mSurface == nullptr) {
      mDiagnostics = "the Metal window surface could not be recreated on its device";
      return false;
    }
    // The RHI grants a format and drawable size; the layer's point-to-pixel scale stays the
    // display's backing scale even while the drawable is render-scaled for a live resize.
    mMetalLayer.contentsScale = mWindow != nil ? mWindow.backingScaleFactor : mDpiScale;
    return true;
  }

  void installApplication() {
    static bool prepared = false;
    static CoralAppDelegate* __strong appDelegate = nil;
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    if (prepared) return;
    prepared = true;

    appDelegate = [[CoralAppDelegate alloc] init];
    NSApp.delegate = appDelegate;

    NSString* appName = NSProcessInfo.processInfo.processName ?: @"Coral UI";

    NSMenu* menuBar = [[NSMenu alloc] init];

    // ---- Application menu ----
    NSMenuItem* applicationItem = [[NSMenuItem alloc] init];
    NSMenu* applicationMenu = [[NSMenu alloc] initWithTitle:appName];
    [applicationMenu addItemWithTitle:[@"About " stringByAppendingString:appName]
                              action:@selector(orderFrontStandardAboutPanel:)
                       keyEquivalent:@""];
    [applicationMenu addItem:[NSMenuItem separatorItem]];

    NSMenu* servicesMenu = [[NSMenu alloc] initWithTitle:@"Services"];
    NSMenuItem* servicesItem = [[NSMenuItem alloc] initWithTitle:@"Services"
                                                         action:nil
                                                  keyEquivalent:@""];
    servicesItem.submenu = servicesMenu;
    [applicationMenu addItem:servicesItem];
    NSApp.servicesMenu = servicesMenu;
    [applicationMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* hide = [applicationMenu addItemWithTitle:[@"Hide " stringByAppendingString:appName]
                                                  action:@selector(hide:)
                                           keyEquivalent:@"h"];
    hide.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    NSMenuItem* hideOthers = [applicationMenu addItemWithTitle:@"Hide Others"
                                                        action:@selector(hideOtherApplications:)
                                                 keyEquivalent:@"h"];
    hideOthers.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    [applicationMenu addItemWithTitle:@"Show All"
                               action:@selector(unhideAllApplications:)
                        keyEquivalent:@""];
    [applicationMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* quit = [applicationMenu addItemWithTitle:[@"Quit " stringByAppendingString:appName]
                                                   action:@selector(terminate:)
                                            keyEquivalent:@"q"];
    quit.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    applicationItem.submenu = applicationMenu;
    [menuBar addItem:applicationItem];

    // ---- File menu ----
    NSMenuItem* fileItem = [[NSMenuItem alloc] init];
    NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    NSMenuItem* close = [fileMenu addItemWithTitle:@"Close"
                                            action:@selector(performClose:)
                                     keyEquivalent:@"w"];
    close.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    fileItem.submenu = fileMenu;
    [menuBar addItem:fileItem];

    // ---- Edit menu ----
    NSMenuItem* editItem = [[NSMenuItem alloc] init];
    NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    NSMenuItem* undo = [editMenu addItemWithTitle:@"Undo"
                                           action:@selector(undo:)
                                    keyEquivalent:@"z"];
    undo.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    NSMenuItem* redo = [editMenu addItemWithTitle:@"Redo"
                                           action:@selector(redo:)
                                    keyEquivalent:@"z"];
    redo.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [editMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* cut = [editMenu addItemWithTitle:@"Cut"
                                          action:@selector(cut:)
                                   keyEquivalent:@"x"];
    cut.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    NSMenuItem* copy = [editMenu addItemWithTitle:@"Copy"
                                           action:@selector(copy:)
                                    keyEquivalent:@"c"];
    copy.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    NSMenuItem* paste = [editMenu addItemWithTitle:@"Paste"
                                            action:@selector(paste:)
                                     keyEquivalent:@"v"];
    paste.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    [editMenu addItemWithTitle:@"Delete" action:@selector(delete:) keyEquivalent:@""];
    [editMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* selectAll = [editMenu addItemWithTitle:@"Select All"
                                                action:@selector(selectAll:)
                                         keyEquivalent:@"a"];
    selectAll.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    editItem.submenu = editMenu;
    [menuBar addItem:editItem];

    // ---- Window menu ----
    NSMenuItem* windowItem = [[NSMenuItem alloc] init];
    NSMenu* windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    NSMenuItem* minimize = [windowMenu addItemWithTitle:@"Minimize"
                                                 action:@selector(performMiniaturize:)
                                          keyEquivalent:@"m"];
    minimize.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [windowMenu addItem:[NSMenuItem separatorItem]];
    [windowMenu addItemWithTitle:@"Bring All to Front"
                          action:@selector(arrangeInFront:)
                   keyEquivalent:@""];
    windowItem.submenu = windowMenu;
    [menuBar addItem:windowItem];
    NSApp.windowsMenu = windowMenu;

    NSApp.mainMenu = menuBar;
    [NSApp finishLaunching];
  }


  static void postWakeEvent() {
    @autoreleasepool {
      NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                         location:NSZeroPoint
                                    modifierFlags:0
                                        timestamp:0
                                     windowNumber:0
                                          context:nil
                                          subtype:0
                                            data1:0
                                            data2:0];
      if (event != nil) [NSApp postEvent:event atStart:YES];
    }
  }

  /** Re-reads the backing pixel size and reports it to the window. @return true when it changed. */
  bool updateBackingSize(const bool notifyHost) {
    if (mView == nil || mWindow == nil) return false;
    const NSRect backing = [mView convertRectToBacking:mView.bounds];
    const float backingScale = static_cast<float>(mWindow.backingScaleFactor);
    const int width = std::max(1, static_cast<int>(std::lround(backing.size.width)));
    const int height = std::max(1, static_cast<int>(std::lround(backing.size.height)));
    mBackingScale = backingScale > 0.0f ? backingScale : 1.0f;
    const float scale = mBackingScale;
    const float previousScale = mDpiScale;
    mDpiScale = scale;
    // The layer's own scale is always the display's backing scale; the drawable is the smaller,
    // render-scale-sized texture and `contentsGravity` scales it into the layer bounds.
    mMetalLayer.contentsScale = backingScale > 0.0f ? backingScale : 1.0f;
    if (width == mWidth && height == mHeight && mDpiScale == previousScale) {
      return false;
    }
    mWidth = width;
    mHeight = height;
    // The swapchain is resized once per frame in `beginFrame`, not here: an AppKit live resize
    // delivers this callback for every intermediate size, and rebuilding the swapchain twice per
    // event (here and again in beginFrame) is what made the drag stutter.
    if (notifyHost && mHost != nullptr && !mAdjustingContentSize) {
      mHost->platformResized(mWidth, mHeight);
    }
    return true;
  }

  Window::Options mOptions;
  backend::Device* mSharedDevice = nullptr;
  int mWidth = 1;
  int mHeight = 1;
  float mDpiScale = 1.0f;
  float mBackingScale = 1.0f;
  bool mMaximized = false;
  bool mMinimized = false;
  bool mFullscreen = false;
  bool mClosing = false;
  bool mAdjustingContentSize = false;
  bool mUsingMetal = true;
  bool mHasInteractiveRender = false;
  double mLastResizeMilliseconds = 0.0;
  double mLastAcquireMilliseconds = 0.0;
  double mLastPresentMilliseconds = 0.0;
  std::chrono::steady_clock::duration mLastInteractiveCost{};
  std::chrono::steady_clock::time_point mNextInteractiveRender{};
  Window::Cursor mCursor = Window::Cursor::Arrow;
  NSCursor* mNativeCursor = nil;
  NSWindow* mWindow = nil;
  CoralUIView* mView = nil;
  CoralUIWindowDelegate* mDelegate = nil;
  CAMetalLayer* mMetalLayer = nil;
  WindowHost* mHost = nullptr;
  std::unique_ptr<graphics::gpu::WindowSurface> mSurface;
  std::unique_ptr<backend::Device> mDeviceOwner;
  std::string mDiagnostics;
};

} // namespace coral::ui::detail

// ==================================================================================================
// The content view
// ==================================================================================================

using coral::ui::detail::CocoaPlatform;

@implementation CoralWindow

- (BOOL)canBecomeKeyWindow {
  // A borderless window answers NO by default, which would leave a client-decorated window unable
  // to take keyboard focus.
  return YES;
}

- (BOOL)canBecomeMainWindow {
  return YES;
}

@end

@implementation CoralAppDelegate

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  (void)app;
  return YES;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  // The framework's Application/Window loops own termination; letting AppKit terminate on the last
  // window close would cut a multi-window session short while another window is being recreated.
  (void)sender;
  return NO;
}

@end

@implementation CoralUIView

@synthesize platform = _platform;
@synthesize coralTrackingArea = _coralTrackingArea;

- (BOOL)isFlipped {
  // Kept unflipped: a flipped content view makes its backing CAMetalLayer geometry-flipped too, and
  // `contentsGravity = topLeft` then anchors the last drawable from the other edge while the window
  // changes height. Input is flipped in `devicePointForEvent`.
  return NO;
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)becomeFirstResponder {
  return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
  // A click on an inactive window should reach the control the user aimed at, not just activate the
  // window and be swallowed.
  (void)event;
  return YES;
}

- (CALayer*)makeBackingLayer {
  return [CAMetalLayer layer];
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  if (self.coralTrackingArea != nil) {
    [self removeTrackingArea:self.coralTrackingArea];
  }
  self.coralTrackingArea = [[NSTrackingArea alloc]
    initWithRect:self.bounds
         options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
           NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
           owner:self
        userInfo:nil];
  [self addTrackingArea:self.coralTrackingArea];
}

- (void)viewDidChangeBackingProperties {
  [super viewDidChangeBackingProperties];
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowDidChangeGeometry();
}

- (void)mouseDown:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return;
  [self.window makeFirstResponder:self];
  platform->dispatchPointerEvent(event, coral::ui::PointerButton::Primary, 1, true);
}

- (void)mouseUp:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::Primary, 2, true);
  }
}

- (void)rightMouseDown:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::Secondary, 1, true);
  }
}

- (void)rightMouseUp:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::Secondary, 2, true);
  }
}

- (void)otherMouseDown:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::Middle, 1, true);
  }
}

- (void)otherMouseUp:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::Middle, 2, true);
  }
}

- (void)mouseMoved:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchPointerEvent(event, coral::ui::PointerButton::None, 0, true);
}

- (void)mouseDragged:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::None, 0, true);
  }
}

- (void)rightMouseDragged:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::None, 0, true);
  }
}

- (void)otherMouseDragged:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::None, 0, true);
  }
}

- (void)mouseEntered:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchPointerEvent(event, coral::ui::PointerButton::None, 0, true);
}

- (void)mouseExited:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchPointerEvent(event, coral::ui::PointerButton::None, 0, false);
}

- (void)scrollWheel:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchScrollEvent(event);
}

- (void)keyDown:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return;
  platform->dispatchKeyEvent(event, true);
  platform->dispatchTextEvent(event);
}

- (void)keyUp:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchKeyEvent(event, false);
}

- (void)flagsChanged:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchModifierEvent(event);
}

// Standard responder-chain actions sent by the native Edit/File menus.
- (void)undo:(id)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->performEditCommand(coral::ui::Key::Z);
}

- (void)redo:(id)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->performEditCommand(coral::ui::Key::Z, true);
}

- (void)cut:(id)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->performEditCommand(coral::ui::Key::X);
}

- (void)copy:(id)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->performEditCommand(coral::ui::Key::C);
}

- (void)paste:(id)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->performEditCommand(coral::ui::Key::V);
}

- (void)delete:(id)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->performEditCommand(coral::ui::Key::Delete, false, false);
}

- (void)selectAll:(id)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->performEditCommand(coral::ui::Key::A);
}

- (void)performClose:(id)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->requestCloseFromMenu();
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return NO;
  const SEL action = item.action;
  if (action == @selector(undo:) || action == @selector(redo:) ||
      action == @selector(cut:) || action == @selector(copy:) ||
      action == @selector(paste:) || action == @selector(delete:) ||
      action == @selector(selectAll:)) {
    return platform->hasTextFocus();
  }
  return YES;
}

- (void)resetCursorRects {
  // The framework's `Window::setCursor` owns the cursor shape; claiming the whole view keeps AppKit
  // from resetting it to the arrow on every tracking-area update.
  [super resetCursorRects];
}

@end

// ==================================================================================================
// The window delegate
// ==================================================================================================

@implementation CoralUIWindowDelegate

@synthesize platform = _platform;

- (BOOL)windowShouldClose:(NSWindow*)sender {
  (void)sender;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) {
    platform->windowShouldCloseRequested();
    return NO;
  }
  return YES;
}

- (void)windowDidResize:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowDidChangeGeometry();
}

- (void)windowWillStartLiveResize:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowWillStartLiveResize();
}

- (void)windowDidEndLiveResize:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowDidEndLiveResize();
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowDidChangeGeometry();
}

- (void)windowDidChangeScreen:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowDidChangeScreen();
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowStateChanged();
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowStateChanged();
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowStateChanged();
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowStateChanged();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowVisibilityChanged(true);
}

- (void)windowDidResignKey:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowVisibilityChanged(false);
}

- (void)windowDidChangeEffectiveAppearance:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->windowAppearanceChanged();
}

@end

// ==================================================================================================
// The factory
// ==================================================================================================

namespace coral::ui::detail {

std::unique_ptr<WindowPlatform> createCocoaPlatform(const Window::Options& options,
    backend::Device* sharedDevice, std::string* diagnostics) {
  auto platform = std::make_unique<CocoaPlatform>(options, sharedDevice);
  if (!platform->initialize(diagnostics)) {
    return nullptr;
  }
  return platform;
}

} // namespace coral::ui::detail
