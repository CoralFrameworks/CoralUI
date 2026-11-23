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
#include "../../ApplicationLifecycleInternal.h"

#include <gpu/WindowSurface.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
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

/** The content view: first responder, pointer tracking, AppKit event translation and IME. */
@interface CoralUIView : NSView <NSTextInputClient>
@property(nonatomic, assign) void* platform;
@property(nonatomic, strong) NSTrackingArea* coralTrackingArea;
/** The marked (composing) text's range in UTF-16 units of the surrounding text. */
@property(nonatomic) NSRange coralMarkedRange;
@end

/** A borderless window still has to accept key/main status, or client-decorated windows lose input. */
@interface CoralWindow : NSWindow
@end

/** Process-wide AppKit delegate. Window delegates stay per-window; this one owns the application. */
@interface CoralAppDelegate : NSObject <NSApplicationDelegate>
@end

/** The source side of an outgoing drag: CoralUI -> Finder / another application. */
@interface CoralDragSource : NSObject <NSDraggingSource>
@property(nonatomic, assign) void* platform;
@property(nonatomic, assign) NSDragOperation allowed;
@end

@interface CoralTitlebarAccessoryTarget : NSObject
@property(nonatomic, assign) void* platform;
@property(nonatomic, strong) NSSegmentedControl* control;
- (void)changed:(id)sender;
@end

@interface CoralSidebarController : NSObject <NSTableViewDataSource, NSTableViewDelegate>
@property(nonatomic, assign) void* platform;
@property(nonatomic, strong) NSArray<NSString*>* identifiers;
@property(nonatomic, strong) NSArray<NSString*>* labels;
@property(nonatomic, strong) NSArray<NSString*>* symbols;
@property(nonatomic, strong) NSIndexSet* groupRows;
@property(nonatomic, strong) NSTableView* table;
@end

/** Forwards split-pane resizes to the platform so the Metal layer follows the content pane. */
@interface CoralSplitViewController : NSSplitViewController
@property(nonatomic, assign) void* platform;
@end

@interface CoralToolbarTarget : NSObject
@property(nonatomic, assign) void* platform;
- (void)activate:(id)sender;
@end

/** Builds and owns the NSToolbar items for one Cocoa window. */
@interface CoralToolbarDelegate : NSObject <NSToolbarDelegate>
@property(nonatomic, assign) void* platform;
@property(nonatomic, strong) NSArray<NSString*>* identifiers;
@property(nonatomic, strong) NSDictionary<NSString*, NSToolbarItem*>* items;
@property(nonatomic, strong) CoralToolbarTarget* target;
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
  /** True when every open Cocoa window's close interceptor allows closing. */
  [[nodiscard]] static bool canCloseAllWindows();
  /** @return true when the quit was started, false when a window vetoed it. */
  static bool requestQuit();
  [[nodiscard]] static bool hasOpenWindows();
  
  CocoaPlatform(Window::Options options, backend::Device* sharedDevice)
    : mOptions(std::move(options)), mSharedDevice(sharedDevice),
      mWidth(std::max(1, mOptions.width)), mHeight(std::max(1, mOptions.height)) {}

  ~CocoaPlatform() override {
    if (mActiveDragSource != nil) {
      mActiveDragSource.platform = nullptr;
      mActiveDragSource = nil;
    }
    mSurface.reset();
    mDeviceOwner.reset();
    mHost = nullptr;
    if (mSecureInputEnabled) {
      DisableSecureEventInput();
      mSecureInputEnabled = false;
    }
    if (mToolbarDelegate != nil) {
      mToolbarDelegate.platform = nullptr;
      mToolbarDelegate.target.platform = nullptr;
    }
    if (mTitlebarTarget != nil) mTitlebarTarget.platform = nullptr;
    if (mTitlebarAccessory != nil && mWindow != nil) {
      [mWindow removeTitlebarAccessoryViewControllerAtIndex:0];
      mTitlebarAccessory = nil;
      mTitlebarControl = nil;
      mTitlebarTarget = nil;
    }
    if (mSplitController != nil) {
      mSplitController.platform = nullptr;
      mWindow.contentViewController = nil;
      mSplitController = nil;
      mSidebarController = nil;
      mSidebarSplitItem = nil;
    }
    if (mWindow != nil) {
      mWindow.toolbar = nil;
      mWindow.delegate = nil;
      [mWindow orderOut:nil];
      [mWindow close];
      mWindow = nil;
    }
    mView = nil;
    mDelegate = nil;
    unregisterWindow(this);
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
    // Native tabbing and frame autosave are window-server features: the same NSWindow API that
    // Finder/AppKit apps use, not a CoralUI-side position cache.
    switch (mOptions.tabbingMode) {
      case Window::TabbingMode::Preferred:
        mWindow.tabbingMode = NSWindowTabbingModePreferred;
        break;
      case Window::TabbingMode::Disallowed:
        mWindow.tabbingMode = NSWindowTabbingModeDisallowed;
        break;
      case Window::TabbingMode::Automatic:
      default:
        mWindow.tabbingMode = NSWindowTabbingModeAutomatic;
        break;
    }
    mWindow.tabbingIdentifier = [NSString stringWithUTF8String:
      (mOptions.appId.empty() ? "com.coral.ui" : mOptions.appId.c_str())];

    BOOL restoredFrame = NO;
    if (!mOptions.frameAutosaveName.empty()) {
      NSString* autosave = [NSString stringWithUTF8String:mOptions.frameAutosaveName.c_str()];
      (void)[mWindow setFrameAutosaveName:autosave];
      restoredFrame = [mWindow setFrameUsingName:autosave];
    }
    if (restoredFrame) {
      // AppKit restored the user's last frame; do not override it with a centred default.
    } else if (mOptions.position.has_value()) {
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
    // Finder files, plain text and URL strings. The view implements the destination side;
    // NSDraggingSource is a later phase.
    [mView registerForDraggedTypes:@[
      NSPasteboardTypeFileURL, NSPasteboardTypeString, NSPasteboardTypeURL
    ]];
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
    NSViewController* content = [[NSViewController alloc] init];
    content.view = mView;
    mSplitController = [[CoralSplitViewController alloc] init];
    mSplitController.platform = this;
    [mSplitController addSplitViewItem:[NSSplitViewItem splitViewItemWithViewController:content]];
    mWindow.contentViewController = mSplitController;
    [mWindow makeFirstResponder:mView];
    [mWindow makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    updateBackingSize(false);

    if (mOptions.renderHost == RenderHostKind::RenderThread) {
      // The render thread owns the surface; `nativeWindow()` hands it this layer.
      registerWindow(this);
      return true;
    }
    if (!createSurface()) {
      if (diagnostics != nullptr) {
        *diagnostics = mDiagnostics.empty()
          ? "the Metal window surface could not be created" : mDiagnostics;
      }
      return false;
    }
    registerWindow(this);
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

  [[nodiscard]] bool supportsRetainedFrames() const override {
    return true;
  }

  bool presentRetained() override {
    return mSurface != nullptr && mSurface->presentRetained();
  }

  [[nodiscard]] bool supportsDamageRects() const override {
    return true;
  }

  bool presentDamage(const Rect& damage) override {
    return mSurface != nullptr && mSurface->presentDamage(damage);
  }

  [[nodiscard]] bool supportsBoundarySubmission() const override {
    // Opt-in until the Metal multi-list path has as much mileage as the Linux window path.
    static const bool enabled = std::getenv("CORAL_UI_FRAME_PLAN_WINDOW") != nullptr;
    return enabled && mSurface != nullptr;
  }

  bool presentBoundaries(
      const std::vector<graphics::gpu::Surface::ListItem>& items) override {
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

  void setTitlebarAccessoryItems(std::vector<Window::TitlebarAccessoryItem> items) override {
    mTitlebarItems = std::move(items);
    applyTitlebarAccessory();
  }

  void setTitlebarAccessorySelectionHandler(std::function<void(int)> handler) override {
    mTitlebarAccessorySelection = std::move(handler);
  }

  void titlebarAccessorySelected(const int index) {
    if (mTitlebarAccessorySelection) mTitlebarAccessorySelection(index);
  }

  void applyTitlebarAccessory() {
    if (mWindow == nil) return;
    if (mTitlebarAccessory != nil) {
      [mWindow removeTitlebarAccessoryViewControllerAtIndex:0];
      mTitlebarAccessory = nil;
      mTitlebarControl = nil;
      mTitlebarTarget = nil;
    }
    if (mTitlebarItems.empty()) return;
    CoralTitlebarAccessoryTarget* target = [[CoralTitlebarAccessoryTarget alloc] init];
    target.platform = this;
    NSSegmentedControl* control = [[NSSegmentedControl alloc] init];
    control.segmentCount = static_cast<NSInteger>(mTitlebarItems.size());
    control.trackingMode = NSSegmentSwitchTrackingSelectOne;
    control.segmentStyle = NSSegmentStyleTexturedRounded;
    for (std::size_t index = 0; index < mTitlebarItems.size(); ++index) {
      const Window::TitlebarAccessoryItem& item = mTitlebarItems[index];
      const NSInteger segment = static_cast<NSInteger>(index);
      [control setLabel:item.label.empty()
        ? [NSString stringWithFormat:@"%zu", index] : [NSString stringWithUTF8String:item.label.c_str()]
        forSegment:segment];
      if (!item.symbol.empty() && @available(macOS 11.0, *)) {
        [control setImage:[NSImage imageWithSystemSymbolName:
          [NSString stringWithUTF8String:item.symbol.c_str()] accessibilityDescription:nil]
               forSegment:segment];
      }
    }
    control.target = target;
    control.action = @selector(changed:);
    [control sizeToFit];
    target.control = control;
    mTitlebarTarget = target;
    mTitlebarControl = control;
    mTitlebarAccessory = [[NSTitlebarAccessoryViewController alloc] init];
    mTitlebarAccessory.view = control;
    mTitlebarAccessory.layoutAttribute = NSLayoutAttributeTrailing;
    [mWindow addTitlebarAccessoryViewController:mTitlebarAccessory];
  }

  void setSidebarItems(std::vector<Window::SidebarItem> items) override {
    mSidebarItems = std::move(items);
    applySidebar();
  }

  void setSidebarVisible(const bool visible) override {
    mSidebarVisible = visible;
    if (mSidebarSplitItem != nil) mSidebarSplitItem.collapsed = !visible;
  }

  void setSidebarSelectionHandler(std::function<void(std::string_view)> handler) override {
    mSidebarSelection = std::move(handler);
  }

  [[nodiscard]] std::string selectedSidebarItem() const override {
    return mSelectedSidebarItem;
  }

  void sidebarSelectionChanged(const std::string_view identifier) {
    mSelectedSidebarItem = std::string(identifier);
    if (mSidebarSelection) mSidebarSelection(identifier);
  }

  void splitViewResized() {
    if (mHost != nullptr) updateBackingSize(true);
  }

  void applySidebar() {
    if (mSplitController == nil) return;
    if (mSidebarItems.empty()) {
      if (mSidebarSplitItem != nil) {
        [mSplitController removeSplitViewItem:mSidebarSplitItem];
        mSidebarSplitItem = nil;
        mSidebarController = nil;
      }
      return;
    }
    if (mSidebarController == nil) {
      mSidebarController = [[CoralSidebarController alloc] init];
      mSidebarController.platform = this;
    }
    NSMutableArray<NSString*>* identifiers = [NSMutableArray arrayWithCapacity:mSidebarItems.size()];
    NSMutableArray<NSString*>* labels = [NSMutableArray arrayWithCapacity:mSidebarItems.size()];
    NSMutableArray<NSString*>* symbols = [NSMutableArray arrayWithCapacity:mSidebarItems.size()];
    NSMutableIndexSet* groupRows = [NSMutableIndexSet indexSet];
    for (std::size_t index = 0; index < mSidebarItems.size(); ++index) {
      const Window::SidebarItem& item = mSidebarItems[index];
      if (item.group) [groupRows addIndex:index];
      [identifiers addObject:item.id.empty()
        ? [NSString stringWithFormat:@"coral.sidebar.%zu", identifiers.count]
        : [NSString stringWithUTF8String:item.id.c_str()]];
      [labels addObject:item.label.empty()
        ? [NSString stringWithUTF8String:item.id.c_str()]
        : [NSString stringWithUTF8String:item.label.c_str()]];
      [symbols addObject:item.symbol.empty() ? @"" :
        [NSString stringWithUTF8String:item.symbol.c_str()]];
    }
    mSidebarController.identifiers = identifiers;
    mSidebarController.labels = labels;
    mSidebarController.symbols = symbols;
    mSidebarController.groupRows = groupRows;
    [mSidebarController.table reloadData];
    if (mSidebarSplitItem == nil) {
      NSViewController* sidebar = [[NSViewController alloc] init];
      NSVisualEffectView* background = [[NSVisualEffectView alloc]
        initWithFrame:NSMakeRect(0.0, 0.0, 220.0, std::max(1.0, mWindow.contentView.bounds.size.height))];
      background.material = NSVisualEffectMaterialSidebar;
      background.blendingMode = NSVisualEffectBlendingModeWithinWindow;
      background.state = NSVisualEffectStateActive;
      background.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
      NSScrollView* scroll = [[NSScrollView alloc] initWithFrame:background.bounds];
      scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
      scroll.hasVerticalScroller = YES;
      scroll.drawsBackground = NO;
      NSTableView* table = [[NSTableView alloc] initWithFrame:scroll.bounds];
      table.headerView = nil;
      table.rowSizeStyle = NSTableViewRowSizeStyleMedium;
      table.selectionHighlightStyle = NSTableViewSelectionHighlightStyleSourceList;
      table.allowsMultipleSelection = NO;
      NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:@"coral.sidebar.column"];
      column.width = 220.0;
      [table addTableColumn:column];
      table.delegate = mSidebarController;
      table.dataSource = mSidebarController;
      mSidebarController.table = table;
      scroll.documentView = table;
      [background addSubview:scroll];
      sidebar.view = background;
      mSidebarSplitItem = [NSSplitViewItem sidebarWithViewController:sidebar];
      mSidebarSplitItem.canCollapse = YES;
      mSidebarSplitItem.minimumThickness = 160.0;
      mSidebarSplitItem.maximumThickness = 320.0;
      [mSplitController insertSplitViewItem:mSidebarSplitItem atIndex:0];
    }
    mSidebarSplitItem.collapsed = !mSidebarVisible;
  }

  void setSecureInputEnabled(const bool enabled) override {
    if (mSecureInputEnabled == enabled) return;
    if (![NSThread isMainThread]) return;
    if (enabled) {
      EnableSecureEventInput();
      mSecureInputEnabled = true;
    } else {
      DisableSecureEventInput();
      mSecureInputEnabled = false;
    }
  }

  [[nodiscard]] bool isSecureInputEnabled() const override {
    return IsSecureEventInputEnabled();
  }

  void setToolbarItems(std::vector<Window::ToolbarItem> items) override {
    mToolbarItems = std::move(items);
    applyToolbar();
  }

  void setToolbarVisible(const bool visible) override {
    mToolbarVisible = visible;
    if (mWindow != nil) mWindow.toolbar = visible ? mToolbar : nil;
  }

  void setToolbarActionHandler(std::function<void(std::string_view)> handler) override {
    mToolbarAction = std::move(handler);
  }

  void toolbarItemActivated(const std::string_view identifier) {
    if (mToolbarAction) mToolbarAction(identifier);
  }

  void applyToolbar() {
    if (mWindow == nil) return;
    if (mToolbar != nil) {
      mWindow.toolbar = nil;
      mToolbar = nil;
      mToolbarDelegate = nil;
    }
    if (mToolbarItems.empty()) return;
    NSMutableArray<NSString*>* identifiers = [NSMutableArray arrayWithCapacity:mToolbarItems.size()];
    NSMutableDictionary<NSString*, NSToolbarItem*>* items = [NSMutableDictionary dictionary];
    CoralToolbarTarget* target = [[CoralToolbarTarget alloc] init];
    target.platform = this;
    for (std::size_t index = 0; index < mToolbarItems.size(); ++index) {
      const Window::ToolbarItem& item = mToolbarItems[index];
      NSString* identifier = item.id.empty()
        ? [NSString stringWithFormat:@"coral.toolbar.%zu", index]
        : [NSString stringWithUTF8String:item.id.c_str()];
      if (identifier.length == 0 || items[identifier] != nil) continue;
      NSToolbarItem* nativeItem = [[NSToolbarItem alloc] initWithItemIdentifier:identifier];
      nativeItem.label = item.label.empty() ? identifier
        : [NSString stringWithUTF8String:item.label.c_str()];
      nativeItem.paletteLabel = nativeItem.label;
      if (!item.tooltip.empty()) {
        nativeItem.toolTip = [NSString stringWithUTF8String:item.tooltip.c_str()];
      }
      if (!item.symbol.empty() && @available(macOS 11.0, *)) {
        nativeItem.image = [NSImage imageWithSystemSymbolName:
          [NSString stringWithUTF8String:item.symbol.c_str()] accessibilityDescription:nil];
      }
      nativeItem.target = target;
      nativeItem.action = @selector(activate:);
      items[identifier] = nativeItem;
      [identifiers addObject:identifier];
    }
    if (identifiers.count == 0) return;
    mToolbarDelegate = [[CoralToolbarDelegate alloc] init];
    mToolbarDelegate.platform = this;
    mToolbarDelegate.identifiers = identifiers;
    mToolbarDelegate.items = items;
    mToolbarDelegate.target = target;
    mToolbar = [[NSToolbar alloc] initWithIdentifier:@"CoralUIToolbar"];
    mToolbar.delegate = mToolbarDelegate;
    mToolbar.displayMode = NSToolbarDisplayModeIconAndLabel;
    mToolbar.allowsUserCustomization = YES;
    mToolbar.autosavesConfiguration = YES;
    if (@available(macOS 11.0, *)) mWindow.toolbarStyle = NSWindowToolbarStyleUnified;
    if (mToolbarVisible) mWindow.toolbar = mToolbar;
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

  [[nodiscard]] float displayHeadroom() const override {
    NSScreen* screen = mWindow != nil && mWindow.screen != nil ? mWindow.screen : [NSScreen mainScreen];
    return screen != nil
      ? static_cast<float>(screen.maximumExtendedDynamicRangeColorComponentValue) : 1.0f;
  }

  [[nodiscard]] bool supportsWideColor() const override {
    NSScreen* screen = mWindow != nil && mWindow.screen != nil ? mWindow.screen : [NSScreen mainScreen];
    if (screen == nil || screen.colorSpace == nil) return false;
    CGColorSpaceRef colorSpace = screen.colorSpace.CGColorSpace;
    if (colorSpace == nullptr) return false;
    CFStringRef name = CGColorSpaceGetName(colorSpace);
    return name != nullptr && CFStringCompare(name, kCGColorSpaceDisplayP3, 0) == kCFCompareEqualTo;
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
    // CoralGraphics now has a macOS display-blank source (CVDisplayLink); the kind is enough for the
    // per-window clock to select it. The RHI's render surface still travels through nativeWindow().
    return graphics::gpu::NativeDisplay{ graphics::gpu::NativeDisplayKind::MacOS, nullptr, nullptr };
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

  static ScrollPhase scrollPhaseFrom(const NSEventPhase phase) {
    if ((phase & NSEventPhaseBegan) != 0) return ScrollPhase::Began;
    if ((phase & NSEventPhaseChanged) != 0) return ScrollPhase::Changed;
    if ((phase & NSEventPhaseEnded) != 0) return ScrollPhase::Ended;
    if ((phase & NSEventPhaseCancelled) != 0) return ScrollPhase::Cancelled;
    if ((phase & NSEventPhaseMayBegin) != 0) return ScrollPhase::MayBegin;
    if ((phase & NSEventPhaseStationary) != 0) return ScrollPhase::Stationary;
    return ScrollPhase::None;
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
    scroll.phase = scrollPhaseFrom(event.phase);
    scroll.momentumPhase = scrollPhaseFrom(event.momentumPhase);
    scroll.precise = event.hasPreciseScrollingDeltas != NO;
    mHost->dispatchScroll(scroll);
  }

  void dispatchGestureEvent(NSEvent* event, const GestureKind kind) {
    if (mHost == nullptr) return;
    if (mWindow != nil && mWindow.inLiveResize) return;
    GestureEvent gesture;
    gesture.kind = kind;
    gesture.position = devicePointForEvent(event);
    const NSEventPhase phase = event.phase;
    gesture.phase = (phase & NSEventPhaseBegan) != 0 ? GesturePhase::Began
      : ((phase & NSEventPhaseEnded) != 0 ? GesturePhase::Ended
      : ((phase & NSEventPhaseCancelled) != 0 ? GesturePhase::Cancelled
      : GesturePhase::Changed));
    if (kind == GestureKind::Magnify) {
      gesture.magnitude = static_cast<float>(event.magnification);
    } else if (kind == GestureKind::Rotate) {
      gesture.magnitude = static_cast<float>(event.rotation);
    } else {
      gesture.deltaX = static_cast<float>(event.deltaX);
      gesture.deltaY = static_cast<float>(event.deltaY);
    }
    mHost->dispatchGesture(gesture);
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
  void onFocusChanged(Layer* focused) override {
    (void)focused;
    if (mView != nil) [mView.inputContext invalidateCharacterCoordinates];
  }

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

  void windowOcclusionChanged(const bool visible) {
    if (mHost != nullptr) mHost->platformOcclusionChanged(visible);
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

  // --- IME / text-service entry points -------------------------------------------------------
  void imeSetComposition(const std::string& text) {
    if (mHost != nullptr) mHost->dispatchComposition(text);
  }

  void imeCommit(const std::string& text) {
    if (mHost != nullptr) mHost->dispatchCommit(text);
  }

  void imeDelete(const int32_t index, const uint32_t length) {
    if (mHost != nullptr) mHost->dispatchDelete(index, length);
  }

  [[nodiscard]] bool imeSurroundingText(std::string* text, size_t* cursor, size_t* anchor) const {
    return mHost != nullptr && mHost->surroundingText(text, cursor, anchor);
  }

  [[nodiscard]] Rect imeCursorRect() const {
    return mHost != nullptr ? mHost->imeCursorRect() : Rect{};
  }

  void performKeyCommand(const Key key, const bool shift = false, const bool control = false,
      const bool alt = false, const bool meta = false) {
    if (mHost == nullptr) return;
    KeyEvent event;
    event.key = key;
    event.shift = shift;
    event.control = control;
    event.alt = alt;
    event.meta = meta;
    mHost->dispatchKey(event, true);
  }

  /**
   * Routes one AppKit external drag callback into the window's layer tree.
   *
   * @param phase 0 enter, 1 over, 2 drop, 3 leave.
   * @return the effect the tree accepted, for the drag cursor / drop result.
   */
  DragEffect externalDrag(const DragPayload& payload, const Point position,
      const DragEffect allowed, const int phase) {
    if (mHost == nullptr) return DragEffect::None;
    if (phase == 3) {
      mHost->dispatchDragLeave();
      return DragEffect::None;
    }
    DragEvent event;
    event.position = position;
    event.payload = payload;
    event.effect = allowed;
    event.external = true;
    if (phase == 0) return mHost->dispatchDragEnter(event);
    if (phase == 1) return mHost->dispatchDragOver(event);
    return mHost->dispatchDragDrop(event);
  }

  /** A small type label used as the drag image when the payload has no file icon. */
  static NSImage* makeDragImage(NSString* text) {
    const NSSize size = NSMakeSize(160.0, 24.0);
    return [NSImage imageWithSize:size flipped:NO drawingHandler:^BOOL(NSRect rect) {
      [[NSColor colorWithCalibratedRed:0.16 green:0.20 blue:0.26 alpha:0.92] setFill];
      [[NSBezierPath bezierPathWithRoundedRect:rect xRadius:6.0 yRadius:6.0] fill];
      NSDictionary* attributes = @{
        NSFontAttributeName: [NSFont systemFontOfSize:12.0],
        NSForegroundColorAttributeName: [NSColor whiteColor]
      };
      NSString* shown = text.length > 24
        ? [[text substringToIndex:24] stringByAppendingString:@"..."] : text;
      [shown drawAtPoint:NSMakePoint(8.0, 4.0) withAttributes:attributes];
      return YES;
    }];
  }

  DragEffect beginDragOut(const DragPayload& payload, const DragEffect allowed) override {
    if (mView == nil || payload.empty()) return DragEffect::None;
    NSEvent* event = [NSApp currentEvent];
    if (event == nil) return DragEffect::None;

    NSMutableArray<NSDraggingItem*>* items = [NSMutableArray array];
    const NSPoint location = [mView convertPoint:event.locationInWindow fromView:nil];
    const NSRect frame = NSMakeRect(location.x - 16.0, location.y - 16.0, 32.0, 32.0);
    NSImage* textImage = nil;

    if (payload.hasType("text/uri-list")) {
      for (const std::string& path: payload.paths()) {
        NSString* file = [NSString stringWithUTF8String:path.c_str()];
        if (file.length == 0) continue;
        NSURL* url = [NSURL fileURLWithPath:file];
        NSDraggingItem* item = [[NSDraggingItem alloc] initWithPasteboardWriter:url];
        NSImage* icon = [[NSWorkspace sharedWorkspace] iconForFile:file];
        [item setDraggingFrame:frame contents:icon];
        [items addObject:item];
      }
    } else if (payload.hasType("text/plain")) {
      NSPasteboardItem* pasteboard = [[NSPasteboardItem alloc] init];
      [pasteboard setString:[NSString stringWithUTF8String:payload.data().c_str()]
                    forType:NSPasteboardTypeString];
      NSDraggingItem* item = [[NSDraggingItem alloc] initWithPasteboardWriter:pasteboard];
      if (textImage == nil) textImage = makeDragImage(
        [NSString stringWithUTF8String:payload.data().c_str()]);
      [item setDraggingFrame:frame contents:textImage];
      [items addObject:item];
    } else {
      NSPasteboardItem* pasteboard = [[NSPasteboardItem alloc] init];
      NSData* data = [NSData dataWithBytes:payload.data().data() length:payload.data().size()];
      NSString* type = [NSString stringWithUTF8String:payload.type().c_str()];
      if (type.length == 0 || ![pasteboard setData:data forType:type]) return DragEffect::None;
      if (payload.hasSecondary()) {
        NSData* secondary = [NSData dataWithBytes:payload.secondaryData().data()
                                           length:payload.secondaryData().size()];
        [pasteboard setData:secondary
                    forType:[NSString stringWithUTF8String:payload.secondaryType().c_str()]];
      }
      NSDraggingItem* item = [[NSDraggingItem alloc] initWithPasteboardWriter:pasteboard];
      NSImage* image = makeDragImage([NSString stringWithUTF8String:payload.type().c_str()]);
      [item setDraggingFrame:frame contents:image];
      [items addObject:item];
    }
    if (items.count == 0) return DragEffect::None;

    CoralDragSource* source = [[CoralDragSource alloc] init];
    source.platform = this;
    source.allowed = allowed == DragEffect::Move ? NSDragOperationMove
      : (allowed == DragEffect::Link ? NSDragOperationLink : NSDragOperationCopy);
    mActiveDragSource = source;
    NSDraggingSession* session = [mView beginDraggingSessionWithItems:items event:event
                                                               source:source];
    if (session == nil) {
      mActiveDragSource = nil;
      return DragEffect::None;
    }
    return allowed == DragEffect::None ? DragEffect::Copy : allowed;
  }

  void dragSourceEnded() {
    // The dragging session still owns the source for the duration of its callback; only detach the
    // platform pointer here and keep the object alive until the next drag or destruction.
    if (mActiveDragSource != nil) mActiveDragSource.platform = nullptr;
  }

  // --- Accessibility bridge queries (NSAccessibilityElement wrappers live in the view) ----------
  [[nodiscard]] AccessibilityNode accessibilitySnapshot() const {
    return mAccessibilityProvider != nullptr ? mAccessibilityProvider() : AccessibilityNode{};
  }

  [[nodiscard]] bool performAccessibilityAction(const std::string& path,
      const AccessibilityAction action, const std::string& value) const {
    return mAccessibilityInvoker != nullptr && mAccessibilityInvoker(path, action, value);
  }

  void setAccessibilityTreeProvider(std::function<AccessibilityNode()> provider,
      AccessibilityActionInvoker invoker) override {
    mAccessibilityProvider = std::move(provider);
    mAccessibilityInvoker = std::move(invoker);
  }

  void raiseAccessibilityFocusEvent() override {
    if (mView == nil) return;
    // Post on the element that now owns focus when the generated AX tree can name it; the view's
    // group is the fallback for an unnamed/generic focused layer.
    id focused = [mView accessibilityFocusedUIElement];
    NSAccessibilityPostNotification(focused != nil ? focused : (id)mView,
      NSAccessibilityFocusedUIElementChangedNotification);
  }

  void raiseAccessibilityPropertyEvent(const std::string_view path,
      const AccessibilityEvent event) override {
    (void)path;
    if (mView == nil) return;
    NSString* notification = NSAccessibilityLayoutChangedNotification;
    switch (event) {
      case AccessibilityEvent::ValueChanged: notification = NSAccessibilityValueChangedNotification; break;
      case AccessibilityEvent::ToggleChanged: notification = NSAccessibilityValueChangedNotification; break;
      case AccessibilityEvent::Selected: notification = NSAccessibilitySelectedChildrenChangedNotification; break;
      case AccessibilityEvent::EnabledChanged: notification = NSAccessibilityValueChangedNotification; break;
      case AccessibilityEvent::ChildrenAdded:
      case AccessibilityEvent::ChildrenRemoved: notification = NSAccessibilityLayoutChangedNotification; break;
    }
    NSAccessibilityPostNotification(mView, notification);
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
    [windowMenu addItemWithTitle:@"Show Tab Bar" action:@selector(toggleTabBar:)
                   keyEquivalent:@""];
    [windowMenu addItemWithTitle:@"Merge All Windows" action:@selector(mergeAllWindows:)
                   keyEquivalent:@""];
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
  static void registerWindow(CocoaPlatform* window);
  static void unregisterWindow(CocoaPlatform* window);

  [[nodiscard]] bool canClose() const {
    return mHost != nullptr && mHost->canClose();
  }

  WindowHost* mHost = nullptr;
  CoralDragSource* mActiveDragSource = nil;
  NSToolbar* mToolbar = nil;
  CoralToolbarDelegate* mToolbarDelegate = nil;
  std::vector<Window::ToolbarItem> mToolbarItems;
  std::function<void(std::string_view)> mToolbarAction;
  bool mToolbarVisible = true;
  bool mSecureInputEnabled = false;
  std::vector<Window::TitlebarAccessoryItem> mTitlebarItems;
  NSTitlebarAccessoryViewController* mTitlebarAccessory = nil;
  NSSegmentedControl* mTitlebarControl = nil;
  CoralTitlebarAccessoryTarget* mTitlebarTarget = nil;
  std::function<void(int)> mTitlebarAccessorySelection;
  std::vector<Window::SidebarItem> mSidebarItems;
  CoralSidebarController* mSidebarController = nil;
  NSSplitViewItem* mSidebarSplitItem = nil;
  CoralSplitViewController* mSplitController = nil;
  std::function<void(std::string_view)> mSidebarSelection;
  std::string mSelectedSidebarItem;
  bool mSidebarVisible = true;
  std::function<AccessibilityNode()> mAccessibilityProvider;
  AccessibilityActionInvoker mAccessibilityInvoker;
  std::unique_ptr<graphics::gpu::WindowSurface> mSurface;
  std::unique_ptr<backend::Device> mDeviceOwner;
  std::string mDiagnostics;
};

} // namespace coral::ui::detail

namespace coral::ui::detail {
namespace {

struct CocoaWindowRegistry {
  std::mutex mutex;
  std::vector<CocoaPlatform*> windows;
  bool quitPending = false;
};

CocoaWindowRegistry& windowRegistry() {
  static CocoaWindowRegistry registry;
  return registry;
}

} // namespace

void CocoaPlatform::registerWindow(CocoaPlatform* window) {
  const std::lock_guard<std::mutex> lock(windowRegistry().mutex);
  windowRegistry().windows.push_back(window);
}

void CocoaPlatform::unregisterWindow(CocoaPlatform* window) {
  bool last = false;
  {
    const std::lock_guard<std::mutex> lock(windowRegistry().mutex);
    auto& windows = windowRegistry().windows;
    windows.erase(std::remove(windows.begin(), windows.end(), window), windows.end());
    last = windows.empty() && windowRegistry().quitPending;
    if (last) windowRegistry().quitPending = false;
  }
  if (last) {
    if ([NSThread isMainThread]) {
      [NSApp replyToApplicationShouldTerminate:YES];
    } else {
      dispatch_async(dispatch_get_main_queue(), ^{
        [NSApp replyToApplicationShouldTerminate:YES];
      });
    }
  }
}

bool CocoaPlatform::canCloseAllWindows() {
  std::vector<CocoaPlatform*> windows;
  {
    const std::lock_guard<std::mutex> lock(windowRegistry().mutex);
    windows = windowRegistry().windows;
  }
  for (CocoaPlatform* window: windows) {
    if (!window->canClose()) return false;
  }
  return true;
}

bool CocoaPlatform::hasOpenWindows() {
  const std::lock_guard<std::mutex> lock(windowRegistry().mutex);
  return !windowRegistry().windows.empty();
}

bool CocoaPlatform::requestQuit() {
  std::vector<CocoaPlatform*> windows;
  {
    const std::lock_guard<std::mutex> lock(windowRegistry().mutex);
    if (windowRegistry().quitPending) return true;
    windowRegistry().quitPending = true;
    windows = windowRegistry().windows;
  }
  for (CocoaPlatform* window: windows) {
    if (!window->canClose()) {
      const std::lock_guard<std::mutex> lock(windowRegistry().mutex);
      windowRegistry().quitPending = false;
      return false;
    }
  }
  for (CocoaPlatform* window: windows) {
    if (window->mHost != nullptr) window->mHost->closeWindow();
  }
  return true;
}

} // namespace coral::ui::detail

// ==================================================================================================
// The content view
// ==================================================================================================

using coral::ui::detail::CocoaPlatform;

// NSAccessibilityScrollToVisibleAction is only declared in the macOS 26 SDK. The action's wire name
// is stable ("AXScrollToVisible"), and spelling it keeps Xcode 16.4 / macOS 15 SDK builds compiling.
static NSString* const kCoralScrollToVisibleAction = @"AXScrollToVisible";

namespace {

NSUInteger utf16Length(const std::string& utf8) {
  if (utf8.empty()) return 0;
  NSString* string = [[NSString alloc] initWithBytes:utf8.data() length:utf8.size()
                                            encoding:NSUTF8StringEncoding];
  return string != nil ? string.length : 0;
}

std::string utf8String(NSString* string) {
  if (string == nil) return {};
  const char* utf8 = string.UTF8String;
  return utf8 != nullptr ? std::string(utf8) : std::string();
}

coral::ui::DragEffect effectFromOperation(const NSDragOperation operation) {
  if ((operation & NSDragOperationCopy) != 0) return coral::ui::DragEffect::Copy;
  if ((operation & NSDragOperationMove) != 0) return coral::ui::DragEffect::Move;
  if ((operation & NSDragOperationLink) != 0) return coral::ui::DragEffect::Link;
  if ((operation & NSDragOperationGeneric) != 0) return coral::ui::DragEffect::Copy;
  return coral::ui::DragEffect::None;
}

NSDragOperation operationFromEffect(const coral::ui::DragEffect effect) {
  switch (effect) {
    case coral::ui::DragEffect::Copy: return NSDragOperationCopy;
    case coral::ui::DragEffect::Move: return NSDragOperationMove;
    case coral::ui::DragEffect::Link: return NSDragOperationLink;
    case coral::ui::DragEffect::None:
    default: return NSDragOperationNone;
  }
}

coral::ui::DragPayload payloadFromPasteboard(NSPasteboard* pasteboard) {
  if (pasteboard == nil) return {};
  NSArray<NSURL*>* urls = [pasteboard readObjectsForClasses:@[ [NSURL class] ]
                                                     options:@{
                                                       NSPasteboardURLReadingFileURLsOnlyKey: @YES
                                                     }];
  if (urls.count > 0) {
    std::vector<std::string> paths;
    paths.reserve(urls.count);
    for (NSURL* url in urls) {
      const char* path = url.path.UTF8String;
      if (path != nullptr) paths.emplace_back(path);
    }
    if (!paths.empty()) {
      coral::ui::DragPayload payload = coral::ui::DragPayload::files(std::move(paths));
      NSString* text = [pasteboard stringForType:NSPasteboardTypeString];
      if (text.length > 0) payload.setSecondary("text/plain", utf8String(text));
      return payload;
    }
  }
  NSString* text = [pasteboard stringForType:NSPasteboardTypeString];
  if (text.length > 0) return coral::ui::DragPayload::text(utf8String(text));
  return {};
}

} // namespace

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

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
  (void)sender;
  if (!coral::ui::detail::dispatchShouldTerminate()) return NSTerminateCancel;
  if (!coral::ui::detail::CocoaPlatform::hasOpenWindows()) return NSTerminateNow;
  return coral::ui::detail::CocoaPlatform::requestQuit()
    ? NSTerminateLater : NSTerminateCancel;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication*)sender hasVisibleWindows:(BOOL)flag {
  (void)sender;
  (void)flag;
  (void)coral::ui::detail::dispatchReopen();
  return YES;
}

- (void)application:(NSApplication*)sender openFiles:(NSArray<NSString*>*)filenames {
  std::vector<std::string> paths;
  paths.reserve(filenames.count);
  for (NSString* filename in filenames) {
    const char* path = filename.UTF8String;
    if (path != nullptr) paths.emplace_back(path);
  }
  (void)coral::ui::detail::dispatchOpenFiles(std::move(paths));
  [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (void)application:(NSApplication*)sender openURLs:(NSArray<NSURL*>*)urls {
  std::vector<std::string> paths;
  paths.reserve(urls.count);
  for (NSURL* url in urls) {
    const char* path = url.path.UTF8String;
    if (path != nullptr) paths.emplace_back(path);
  }
  (void)coral::ui::detail::dispatchOpenFiles(std::move(paths));
  (void)sender;
}

@end

@implementation CoralTitlebarAccessoryTarget

@synthesize platform = _platform;
@synthesize control = _control;

- (void)changed:(id)sender {
  (void)sender;
  if (_platform == nullptr || _control == nil) return;
  static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->titlebarAccessorySelected(
    static_cast<int>(_control.selectedSegment));
}

@end

@implementation CoralSidebarController

@synthesize platform = _platform;
@synthesize identifiers = _identifiers;
@synthesize labels = _labels;
@synthesize symbols = _symbols;
@synthesize groupRows = _groupRows;
@synthesize table = _table;

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  (void)tableView;
  return static_cast<NSInteger>(_labels.count);
}

- (NSView*)tableView:(NSTableView*)tableView viewForTableColumn:(NSTableColumn*)column
    row:(NSInteger)row {
  (void)column;
  if (row < 0 || row >= static_cast<NSInteger>(_labels.count)) return nil;
  NSString* identifier = @"coral.sidebar.cell";
  NSTableCellView* cell = [tableView makeViewWithIdentifier:identifier owner:self];
  if (cell == nil) {
    cell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 220.0, 28.0)];
    cell.identifier = identifier;
    NSImageView* image = [[NSImageView alloc] initWithFrame:NSMakeRect(6.0, 6.0, 16.0, 16.0)];
    image.imageScaling = NSImageScaleProportionallyDown;
    [cell addSubview:image];
    cell.imageView = image;
    NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(28.0, 5.0, 184.0, 18.0)];
    label.bezeled = NO;
    label.drawsBackground = NO;
    label.editable = NO;
    label.selectable = NO;
    label.font = [NSFont systemFontOfSize:13.0];
    [cell addSubview:label];
    cell.textField = label;
  }
  cell.textField.stringValue = _labels[static_cast<NSUInteger>(row)];
  const bool group = _groupRows != nil && [_groupRows containsIndex:static_cast<NSUInteger>(row)];
  cell.textField.font = group ? [NSFont boldSystemFontOfSize:11.0]
                              : [NSFont systemFontOfSize:13.0];
  NSString* symbol = group ? @"" : _symbols[static_cast<NSUInteger>(row)];
  if (symbol.length > 0 && @available(macOS 11.0, *)) {
    cell.imageView.image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:nil];
  } else {
    cell.imageView.image = nil;
  }
  return cell;
}

- (BOOL)tableView:(NSTableView*)tableView isGroupRow:(NSInteger)row {
  (void)tableView;
  return _groupRows != nil && [_groupRows containsIndex:static_cast<NSUInteger>(row)];
}

- (BOOL)tableView:(NSTableView*)tableView shouldSelectRow:(NSInteger)row {
  (void)tableView;
  return _groupRows == nil || ![_groupRows containsIndex:static_cast<NSUInteger>(row)];
}

- (void)tableViewSelectionDidChange:(NSNotification*)notification {
  (void)notification;
  if (_platform == nullptr || _table == nil) return;
  const NSInteger row = _table.selectedRow;
  if (row < 0 || row >= static_cast<NSInteger>(_identifiers.count)) return;
  const char* identifier = _identifiers[static_cast<NSUInteger>(row)].UTF8String;
  if (identifier != nullptr) {
    static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->sidebarSelectionChanged(identifier);
  }
}

@end

@implementation CoralSplitViewController

@synthesize platform = _platform;

- (void)splitViewDidResizeSubviews:(NSNotification*)notification {
  [super splitViewDidResizeSubviews:notification];
  if (_platform != nullptr) {
    static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->splitViewResized();
  }
}

@end

@implementation CoralToolbarTarget

@synthesize platform = _platform;

- (void)activate:(id)sender {
  if (_platform == nullptr) return;
  NSString* identifier = [sender itemIdentifier];
  const char* utf8 = identifier.UTF8String;
  static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->toolbarItemActivated(
    utf8 != nullptr ? std::string_view(utf8) : std::string_view());
}

@end

@implementation CoralToolbarDelegate

@synthesize platform = _platform;
@synthesize identifiers = _identifiers;
@synthesize items = _items;
@synthesize target = _target;

- (NSToolbarItem*)toolbar:(NSToolbar*)toolbar itemForItemIdentifier:
    (NSToolbarItemIdentifier)identifier willBeInsertedIntoToolbar:(BOOL)flag {
  (void)toolbar;
  (void)flag;
  return _items[identifier];
}

- (NSArray<NSString*>*)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar {
  (void)toolbar;
  return _identifiers;
}

- (NSArray<NSString*>*)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar {
  (void)toolbar;
  return _identifiers;
}

@end

@implementation CoralDragSource

@synthesize platform = _platform;
@synthesize allowed = _allowed;

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  (void)session;
  (void)context;
  return _allowed != NSDragOperationNone ? _allowed : NSDragOperationCopy;
}

- (BOOL)ignoreModifierKeysForDraggingSession:(NSDraggingSession*)session {
  (void)session;
  return YES;
}

- (void)draggingSession:(NSDraggingSession*)session endedAtPoint:(NSPoint)screenPoint
    operation:(NSDragOperation)operation {
  (void)session;
  (void)screenPoint;
  (void)operation;
  if (_platform != nullptr) {
    static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->dragSourceEnded();
    _platform = nullptr;
  }
}

@end

// ==================================================================================================
// NSAccessibility bridge
// ==================================================================================================

@class CoralUIView;

namespace {

struct CoralAXPayload {
  coral::ui::AccessibilityNode node;
  NSRect logicalBounds = NSZeroRect;
};

NSString* accessibilityRoleFor(const coral::ui::AccessibilityRole role) {
  switch (role) {
    case coral::ui::AccessibilityRole::Button: return NSAccessibilityButtonRole;
    case coral::ui::AccessibilityRole::CheckBox: return NSAccessibilityCheckBoxRole;
    case coral::ui::AccessibilityRole::Radio: return NSAccessibilityRadioButtonRole;
    case coral::ui::AccessibilityRole::Slider: return NSAccessibilitySliderRole;
    case coral::ui::AccessibilityRole::TextField: return NSAccessibilityTextFieldRole;
    case coral::ui::AccessibilityRole::Label: return NSAccessibilityStaticTextRole;
    case coral::ui::AccessibilityRole::List: return NSAccessibilityListRole;
    case coral::ui::AccessibilityRole::Tree: return NSAccessibilityOutlineRole;
    case coral::ui::AccessibilityRole::Menu: return NSAccessibilityMenuRole;
    case coral::ui::AccessibilityRole::Table: return NSAccessibilityTableRole;
    case coral::ui::AccessibilityRole::ScrollView: return NSAccessibilityScrollAreaRole;
    case coral::ui::AccessibilityRole::ListItem: return NSAccessibilityRowRole;
    case coral::ui::AccessibilityRole::Dialog:
    case coral::ui::AccessibilityRole::Group:
    case coral::ui::AccessibilityRole::Generic:
    default: return NSAccessibilityGroupRole;
  }
}

} // namespace

@interface CoralAXElement : NSAccessibilityElement
@property(nonatomic, assign) void* coralPayload;
@property(nonatomic, weak) CoralUIView* coralOwner;
@property(nonatomic, weak) id coralParent;
@end

CoralAXElement* makeCoralAXElement(const coral::ui::AccessibilityNode& node, NSRect logicalBounds,
    CoralUIView* owner, id parent) {
  CoralAXElement* element = [[CoralAXElement alloc] init];
  element.coralPayload = new CoralAXPayload{ node, logicalBounds };
  element.coralOwner = owner;
  element.coralParent = parent;
  return element;
}

@implementation CoralAXElement

- (void)dealloc {
  delete static_cast<CoralAXPayload*>(_coralPayload);
  _coralPayload = nullptr;
}

- (BOOL)isAccessibilityElement {
  return YES;
}

- (NSString*)accessibilityRole {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr) return NSAccessibilityGroupRole;
  // A layer inside a grid/table carries its cell coordinates in the snapshot; that is more specific
  // than its paint role (usually a generic container or label).
  if (payload->node.grid.has_value() && payload->node.grid->row >= 0 &&
      payload->node.grid->column >= 0) {
    return NSAccessibilityCellRole;
  }
  return accessibilityRoleFor(payload->node.role);
}

- (NSString*)accessibilitySubrole {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  return payload != nullptr && payload->node.role == coral::ui::AccessibilityRole::Dialog
    ? NSAccessibilityDialogSubrole : nil;
}

- (NSString*)accessibilityLabel {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr) return nil;
  if (!payload->node.name.empty()) {
    return [NSString stringWithUTF8String:payload->node.name.c_str()];
  }
  if (!payload->node.description.empty()) {
    return [NSString stringWithUTF8String:payload->node.description.c_str()];
  }
  return nil;
}

- (NSString*)accessibilityHelp {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  return payload != nullptr && !payload->node.description.empty()
    ? [NSString stringWithUTF8String:payload->node.description.c_str()] : nil;
}

- (id)accessibilityValue {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr) return nil;
  switch (payload->node.role) {
    case coral::ui::AccessibilityRole::CheckBox:
    case coral::ui::AccessibilityRole::Radio:
      return @(payload->node.selected);
    case coral::ui::AccessibilityRole::Slider:
      return payload->node.range.has_value() ? @(payload->node.range->value) : nil;
    case coral::ui::AccessibilityRole::TextField:
    case coral::ui::AccessibilityRole::Label:
      return payload->node.value.empty() ? nil
        : [NSString stringWithUTF8String:payload->node.value.c_str()];
    default:
      return payload->node.selected ? @YES : nil;
  }
}

- (NSInteger)accessibilityRowCount {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  return payload != nullptr && payload->node.grid.has_value() ? payload->node.grid->rows : 0;
}

- (NSInteger)accessibilityColumnCount {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  return payload != nullptr && payload->node.grid.has_value() ? payload->node.grid->columns : 0;
}

- (NSRange)accessibilityRowIndexRange {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr || !payload->node.grid.has_value() || payload->node.grid->row < 0) {
    return NSMakeRange(NSNotFound, 0);
  }
  return NSMakeRange(static_cast<NSUInteger>(payload->node.grid->row), 1);
}

- (NSRange)accessibilityColumnIndexRange {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr || !payload->node.grid.has_value() || payload->node.grid->column < 0) {
    return NSMakeRange(NSNotFound, 0);
  }
  return NSMakeRange(static_cast<NSUInteger>(payload->node.grid->column), 1);
}

- (BOOL)accessibilityEditable {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  return payload != nullptr && payload->node.editable;
}

- (NSRange)accessibilitySelectedTextRange {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr || !payload->node.textSelection.has_value()) return NSMakeRange(NSNotFound, 0);
  const int start = std::max(0, payload->node.textSelection->start);
  const int end = std::max(start, payload->node.textSelection->end);
  return NSMakeRange(static_cast<NSUInteger>(start), static_cast<NSUInteger>(end - start));
}

- (NSString*)accessibilitySelectedText {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr || payload->node.value.empty() || !payload->node.textSelection.has_value()) {
    return nil;
  }
  NSString* text = [NSString stringWithUTF8String:payload->node.value.c_str()];
  const NSRange range = [self accessibilitySelectedTextRange];
  if (text == nil || range.location == NSNotFound || NSMaxRange(range) > text.length) return nil;
  return [text substringWithRange:range];
}

- (void)setAccessibilityValue:(id)value {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  CoralUIView* owner = _coralOwner;
  if (payload == nullptr || owner == nil || !payload->node.editable) return;
  auto* platform = static_cast<coral::ui::detail::CocoaPlatform*>(owner.platform);
  if (platform == nullptr) return;
  NSString* text = [value isKindOfClass:[NSString class]] ? (NSString*)value
    : ([value respondsToSelector:@selector(stringValue)] ? [value stringValue] : nil);
  if (text == nil) return;
  (void)platform->performAccessibilityAction(payload->node.path,
    coral::ui::AccessibilityAction::SetValue, utf8String(text));
}

- (void)setAccessibilitySelectedTextRange:(NSRange)range {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  CoralUIView* owner = _coralOwner;
  if (payload == nullptr || owner == nil || !payload->node.editable) return;
  auto* platform = static_cast<coral::ui::detail::CocoaPlatform*>(owner.platform);
  if (platform == nullptr) return;
  const std::string value = std::to_string(range.location) + "," + std::to_string(NSMaxRange(range));
  (void)platform->performAccessibilityAction(payload->node.path,
    coral::ui::AccessibilityAction::Select, value);
}

- (BOOL)accessibilityEnabled {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  return payload == nullptr ? NO : payload->node.enabled;
}

- (BOOL)isAccessibilityFocused {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  return payload != nullptr && payload->node.focused;
}

- (id)accessibilityParent {
  return _coralParent;
}

- (NSRect)accessibilityFrame {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  CoralUIView* owner = _coralOwner;
  if (payload == nullptr || owner == nil || owner.window == nil) return NSZeroRect;
  const NSRect logical = payload->logicalBounds;
  const NSPoint point = [owner convertPoint:NSMakePoint(logical.origin.x,
    owner.bounds.size.height - NSMaxY(logical)) toView:nil];
  const NSRect windowRect = NSMakeRect(point.x, point.y, logical.size.width, logical.size.height);
  return [owner.window convertRectToScreen:windowRect];
}

- (NSArray*)accessibilityChildren {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr) return @[];
  NSMutableArray* children = [NSMutableArray arrayWithCapacity:payload->node.children.size()];
  for (const coral::ui::AccessibilityNode& child: payload->node.children) {
    const NSRect childBounds = NSMakeRect(
      payload->logicalBounds.origin.x + child.bounds.getLeft(),
      payload->logicalBounds.origin.y + child.bounds.getTop(),
      std::max(1.0f, child.bounds.getWidth()), std::max(1.0f, child.bounds.getHeight()));
    [children addObject:makeCoralAXElement(child, childBounds, _coralOwner, self)];
  }
  return children;
}

- (NSArray<NSString*>*)accessibilityActionNames {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  if (payload == nullptr) return @[];
  NSMutableArray<NSString*>* actions = [NSMutableArray array];
  const auto& node = payload->node;
  if (coral::ui::hasAccessibilityAction(node.actions, coral::ui::AccessibilityAction::Invoke) ||
      coral::ui::hasAccessibilityAction(node.actions, coral::ui::AccessibilityAction::Toggle) ||
      coral::ui::hasAccessibilityAction(node.actions, coral::ui::AccessibilityAction::Select)) {
    [actions addObject:NSAccessibilityPressAction];
  }
  if (node.range.has_value()) {
    [actions addObject:NSAccessibilityIncrementAction];
    [actions addObject:NSAccessibilityDecrementAction];
  }
  if (coral::ui::hasAccessibilityAction(node.actions, coral::ui::AccessibilityAction::Scroll)) {
    [actions addObject:kCoralScrollToVisibleAction];
  }
  return actions;
}

- (void)accessibilityPerformAction:(NSString*)action {
  const auto* payload = static_cast<const CoralAXPayload*>(_coralPayload);
  CoralUIView* owner = _coralOwner;
  if (payload == nullptr || owner == nil) return;
  auto* platform = static_cast<coral::ui::detail::CocoaPlatform*>(owner.platform);
  if (platform == nullptr) return;
  const std::string path = payload->node.path;
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    coral::ui::AccessibilityAction kind = coral::ui::AccessibilityAction::Invoke;
    if (coral::ui::hasAccessibilityAction(payload->node.actions,
          coral::ui::AccessibilityAction::Toggle)) {
      kind = coral::ui::AccessibilityAction::Toggle;
    } else if (coral::ui::hasAccessibilityAction(payload->node.actions,
                 coral::ui::AccessibilityAction::Select)) {
      kind = coral::ui::AccessibilityAction::Select;
    }
    (void)platform->performAccessibilityAction(path, kind, {});
  } else if ([action isEqualToString:NSAccessibilityIncrementAction] ||
             [action isEqualToString:NSAccessibilityDecrementAction]) {
    if (payload->node.range.has_value()) {
      const auto& range = *payload->node.range;
      const double step = range.step > 0.0 ? range.step : std::max(0.01, (range.maximum - range.minimum) / 100.0);
      const double next = std::clamp(range.value +
        ([action isEqualToString:NSAccessibilityIncrementAction] ? step : -step),
        range.minimum, range.maximum);
      (void)platform->performAccessibilityAction(path,
        coral::ui::AccessibilityAction::SetValue, std::to_string(next));
    }
  } else if ([action isEqualToString:kCoralScrollToVisibleAction]) {
    (void)platform->performAccessibilityAction(path, coral::ui::AccessibilityAction::Scroll, {});
  }
}

@end


@implementation CoralUIView

@synthesize platform = _platform;
@synthesize coralTrackingArea = _coralTrackingArea;
@synthesize coralMarkedRange = _coralMarkedRange;

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
  // Text entry goes through AppKit's input context so an IME can compose, while shortcuts and
  // non-text focus keep the direct KeyEvent path. `interpretKeyEvents:` then calls insertText: or
  // doCommandBySelector:, which this view maps back into the same TextInput operations.
  const NSEventModifierFlags flags = event.modifierFlags;
  const bool shortcut = (flags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) != 0;
  if (!shortcut && platform->hasTextFocus()) {
    [self interpretKeyEvents:@[ event ]];
    return;
  }
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

// ==================================================================================================
// NSTextInputClient: AppKit IME / marked text, mapped onto WindowHost's text hooks
// ==================================================================================================

- (BOOL)hasMarkedText {
  return _coralMarkedRange.length > 0;
}

- (NSRange)markedRange {
  return _coralMarkedRange.length > 0 ? _coralMarkedRange : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return NSMakeRange(NSNotFound, 0);
  std::string text;
  size_t cursor = 0;
  size_t anchor = 0;
  if (!platform->imeSurroundingText(&text, &cursor, &anchor)) return NSMakeRange(NSNotFound, 0);
  const size_t start = std::min(cursor, anchor);
  const size_t end = std::max(cursor, anchor);
  const size_t clampedStart = std::min(start, text.size());
  const size_t clampedEnd = std::min(end, text.size());
  const NSUInteger location = utf16Length(text.substr(0, clampedStart));
  const NSUInteger length = utf16Length(text.substr(0, clampedEnd)) - location;
  return NSMakeRange(location, length);
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange
    replacementRange:(NSRange)replacementRange {
  (void)selectedRange;
  (void)replacementRange;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return;
  NSString* text = [string isKindOfClass:[NSAttributedString class]]
    ? [(NSAttributedString*)string string] : (NSString*)string;
  if (text.length == 0) {
    [self unmarkText];
    return;
  }
  const NSRange selection = [self selectedRange];
  const NSUInteger base = selection.location != NSNotFound ? selection.location : 0;
  _coralMarkedRange = NSMakeRange(base, utf16Length(utf8String(text)));
  platform->imeSetComposition(utf8String(text));
}

- (void)unmarkText {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->imeSetComposition(std::string());
  _coralMarkedRange = NSMakeRange(NSNotFound, 0);
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  (void)replacementRange;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return;
  NSString* text = [string isKindOfClass:[NSAttributedString class]]
    ? [(NSAttributedString*)string string] : (NSString*)string;
  _coralMarkedRange = NSMakeRange(NSNotFound, 0);
  if (text.length == 0) {
    platform->imeSetComposition(std::string());
    return;
  }
  platform->imeCommit(utf8String(text));
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:(NSRangePointer)actualRange {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  std::string text;
  size_t cursor = 0;
  size_t anchor = 0;
  if (platform == nullptr || !platform->imeSurroundingText(&text, &cursor, &anchor)) return nil;
  NSString* surrounding = [[NSString alloc] initWithBytes:text.data() length:text.size()
                                                 encoding:NSUTF8StringEncoding];
  if (surrounding == nil || range.location == NSNotFound ||
      NSMaxRange(range) > surrounding.length) {
    return nil;
  }
  if (actualRange != nullptr) *actualRange = range;
  return [[NSAttributedString alloc] initWithString:[surrounding substringWithRange:range]];
}

- (NSArray<NSString*>*)validAttributesForMarkedText {
  return @[ NSUnderlineStyleAttributeName ];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
  (void)range;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr || self.window == nil) return NSZeroRect;
  const coral::ui::Rect caret = platform->imeCursorRect();
  if (caret.getWidth() <= 0.0f && caret.getHeight() <= 0.0f) return NSZeroRect;
  if (actualRange != nullptr) *actualRange = NSMakeRange(NSNotFound, 0);
  // imeCursorRect is top-left logical; the view is unflipped.
  const NSPoint viewPoint = NSMakePoint(caret.getLeft(),
    self.bounds.size.height - caret.getBottom());
  const NSPoint windowPoint = [self convertPoint:viewPoint toView:nil];
  const NSRect windowRect = NSMakeRect(windowPoint.x, windowPoint.y,
    std::max(1.0f, caret.getWidth()), std::max(1.0f, caret.getHeight()));
  return [self.window convertRectToScreen:windowRect];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
  (void)point;
  return NSNotFound;
}

- (void)doCommandBySelector:(SEL)selector {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) {
    [super doCommandBySelector:selector];
    return;
  }
  if (selector == @selector(deleteBackward:)) {
    platform->performKeyCommand(coral::ui::Key::Backspace);
  } else if (selector == @selector(deleteForward:)) {
    platform->performKeyCommand(coral::ui::Key::Delete);
  } else if (selector == @selector(deleteWordBackward:)) {
    platform->performKeyCommand(coral::ui::Key::Backspace, false, false, true);
  } else if (selector == @selector(deleteWordForward:)) {
    platform->performKeyCommand(coral::ui::Key::Delete, false, false, true);
  } else if (selector == @selector(moveLeft:)) {
    platform->performKeyCommand(coral::ui::Key::Left);
  } else if (selector == @selector(moveRight:)) {
    platform->performKeyCommand(coral::ui::Key::Right);
  } else if (selector == @selector(moveUp:)) {
    platform->performKeyCommand(coral::ui::Key::Up);
  } else if (selector == @selector(moveDown:)) {
    platform->performKeyCommand(coral::ui::Key::Down);
  } else if (selector == @selector(moveWordLeft:)) {
    platform->performKeyCommand(coral::ui::Key::Left, false, false, true);
  } else if (selector == @selector(moveWordRight:)) {
    platform->performKeyCommand(coral::ui::Key::Right, false, false, true);
  } else if (selector == @selector(moveToBeginningOfLine:)) {
    platform->performKeyCommand(coral::ui::Key::Home);
  } else if (selector == @selector(moveToEndOfLine:)) {
    platform->performKeyCommand(coral::ui::Key::End);
  } else if (selector == @selector(moveToBeginningOfDocument:)) {
    platform->performKeyCommand(coral::ui::Key::Home);
  } else if (selector == @selector(moveToEndOfDocument:)) {
    platform->performKeyCommand(coral::ui::Key::End);
  } else if (selector == @selector(pageUp:) || selector == @selector(scrollPageUp:)) {
    platform->performKeyCommand(coral::ui::Key::PageUp);
  } else if (selector == @selector(pageDown:) || selector == @selector(scrollPageDown:)) {
    platform->performKeyCommand(coral::ui::Key::PageDown);
  } else if (selector == @selector(insertNewline:)) {
    platform->performKeyCommand(coral::ui::Key::Enter);
  } else if (selector == @selector(insertTab:)) {
    platform->performKeyCommand(coral::ui::Key::Tab);
  } else if (selector == @selector(insertBacktab:)) {
    platform->performKeyCommand(coral::ui::Key::Tab, true);
  } else if (selector == @selector(cancelOperation:)) {
    platform->performKeyCommand(coral::ui::Key::Escape);
  } else {
    [super doCommandBySelector:selector];
  }
}

// ==================================================================================================
// NSDraggingDestination: external files/text into the layer tree
// ==================================================================================================

- (coral::ui::DragEffect)coralDrag:(id<NSDraggingInfo>)sender phase:(int)phase {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return coral::ui::DragEffect::None;
  const NSPoint windowPoint = [sender draggingLocation];
  const NSPoint viewPoint = [self convertPoint:windowPoint fromView:nil];
  const coral::ui::Point logical{ static_cast<float>(viewPoint.x),
    static_cast<float>(self.bounds.size.height - viewPoint.y) };
  coral::ui::DragPayload payload = phase == 3
    ? coral::ui::DragPayload{} : payloadFromPasteboard([sender draggingPasteboard]);
  const coral::ui::DragEffect allowed =
    effectFromOperation([sender draggingSourceOperationMask]);
  return platform->externalDrag(payload, logical, allowed, phase);
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  return operationFromEffect([self coralDrag:sender phase:0]);
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  return operationFromEffect([self coralDrag:sender phase:1]);
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  (void)[self coralDrag:sender phase:3];
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender {
  (void)sender;
  return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  return [self coralDrag:sender phase:2] != coral::ui::DragEffect::None;
}

- (BOOL)wantsPeriodicDraggingUpdates {
  return NO;
}

// ==================================================================================================
// NSAccessibility root: expose the layer tree as one group per window
// ==================================================================================================

- (BOOL)isAccessibilityElement {
  // The root is a container; the generated CoralAXElement tree carries the roles and values.
  return NO;
}

- (NSString*)accessibilityRole {
  return NSAccessibilityGroupRole;
}

- (NSArray*)accessibilityChildren {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return @[];
  const coral::ui::AccessibilityNode snapshot = platform->accessibilitySnapshot();
  if (snapshot.name.empty() && snapshot.description.empty() && snapshot.children.empty()) return @[];
  const NSRect logical = NSMakeRect(0.0, 0.0, self.bounds.size.width, self.bounds.size.height);
  return @[ makeCoralAXElement(snapshot, logical, self, self) ];
}

- (id)accessibilityHitTest:(NSPoint)point {
  NSArray* roots = [self accessibilityChildren];
  id best = nil;
  NSMutableArray* pending = [NSMutableArray arrayWithArray:roots];
  while (pending.count > 0) {
    id element = pending.firstObject;
    [pending removeObjectAtIndex:0];
    if (!NSPointInRect(point, [element accessibilityFrame])) continue;
    best = element;
    NSArray* children = [element accessibilityChildren];
    if (children.count > 0) [pending addObjectsFromArray:children];
  }
  return best != nil ? best : (id)self;
}

- (id)accessibilityFocusedUIElement {
  NSArray* children = [self accessibilityChildren];
  NSMutableArray* pending = [NSMutableArray arrayWithArray:children];
  while (pending.count > 0) {
    id element = pending.firstObject;
    [pending removeObjectAtIndex:0];
    if ([element isAccessibilityFocused]) return element;
    NSArray* nested = [element accessibilityChildren];
    if (nested.count > 0) [pending addObjectsFromArray:nested];
  }
  return children.firstObject;
}

- (id)accessibilityParent {
  return self.window != nil ? (id)self.window : (id)[NSNull null];
}

- (void)magnifyWithEvent:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchGestureEvent(event, coral::ui::GestureKind::Magnify);
}

- (void)rotateWithEvent:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchGestureEvent(event, coral::ui::GestureKind::Rotate);
}

- (void)swipeWithEvent:(NSEvent*)event {
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform != nullptr) platform->dispatchGestureEvent(event, coral::ui::GestureKind::Swipe);
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

- (void)windowDidChangeOcclusionState:(NSNotification*)notification {
  (void)notification;
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return;
  NSWindow* window = [notification object];
  const bool visible = window != nil &&
    (window.occlusionState & NSWindowOcclusionStateVisible) != 0;
  platform->windowOcclusionChanged(visible);
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
