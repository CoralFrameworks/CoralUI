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
#include "../../WindowRestorationInternal.h"

#include <ui/Command.h>

#include <gpu/WindowSurface.h>
#include <graphics/Image.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <dispatch/dispatch.h>

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

/** `CORAL_UI_AX=0` disables the AppKit accessibility bridge for profiling or host conflicts. */
bool cocoaAccessibilityEnabled() {
  static const bool enabled = [] {
    const char* value = std::getenv("CORAL_UI_AX");
    return value == nullptr || std::string(value) != "0";
  }();
  return enabled;
}

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

static NSMenu* __strong gCoralDockMenu = nil;

/** The source side of an outgoing drag: CoralUI -> Finder / another application. */
@interface CoralDragSource : NSObject <NSDraggingSource>
@property(nonatomic, assign) void* platform;
@property(nonatomic, assign) NSDragOperation allowed;
@end

@interface CoralCommandTarget : NSObject
@property(nonatomic, assign) void* platform;
@property(nonatomic, copy) NSString* commandName;
- (void)invokeCommand:(id)sender;
@end

@interface CoralTitlebarAccessoryTarget : NSObject
@property(nonatomic, assign) void* platform;
@property(nonatomic, strong) NSSegmentedControl* control;
- (void)changed:(id)sender;
@end

@interface CoralSidebarNode : NSObject
@property(nonatomic, copy) NSString* identifier;
@property(nonatomic, copy) NSString* label;
@property(nonatomic, copy) NSString* symbol;
@property(nonatomic, assign) BOOL group;
@property(nonatomic, assign) BOOL editable;
@property(nonatomic, strong) NSArray<CoralSidebarNode*>* children;
@end

@interface CoralSidebarContextAction : NSObject
@property(nonatomic, copy) NSString* identifier;
@property(nonatomic, copy) NSString* title;
@end

@interface CoralSidebarController : NSObject <NSOutlineViewDataSource, NSOutlineViewDelegate, NSMenuDelegate>
@property(nonatomic, assign) void* platform;
@property(nonatomic, strong) NSArray<CoralSidebarNode*>* roots;
@property(nonatomic, strong) NSOutlineView* outline;
@property(nonatomic, strong) NSArray<CoralSidebarContextAction*>* contextActions;
- (void)updateContextMenu;
@end

/** Forwards split-pane resizes to the platform so the Metal layer follows the content pane. */
@interface CoralSplitViewController : NSSplitViewController
@property(nonatomic, assign) void* platform;
@end

@interface CoralToolbarTarget : NSObject
@property(nonatomic, assign) void* platform;
- (void)activate:(id)sender;
@end

@interface CoralNativeMenuTarget : NSObject
@property(nonatomic, assign) void* platform;
- (void)select:(NSMenuItem*)item;
@end

/** A 1px vertical separator that survives NSToolbar unified-style merging. */
@interface CoralToolbarSeparatorView : NSView
@end

@implementation CoralToolbarSeparatorView

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  [[NSColor separatorColor] setFill];
  const NSRect line = NSMakeRect(NSMidX(self.bounds) - 0.5, 4.0, 1.0,
    MAX(1.0, self.bounds.size.height - 8.0));
  NSRectFill(line);
}

@end

/** Builds and owns the NSToolbar items for one Cocoa window. */
@interface CoralToolbarDelegate : NSObject <NSToolbarDelegate>
@property(nonatomic, assign) void* platform;
@property(nonatomic, strong) NSArray<NSString*>* identifiers;
@property(nonatomic, strong) NSDictionary<NSString*, NSToolbarItem*>* items;
@property(nonatomic, strong) CoralToolbarTarget* target;
@end

@interface CoralWindowRestoration : NSObject <NSWindowRestoration>
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
    if (mCommandRegistry != nullptr && mCommandToken != 0) {
      mCommandRegistry->removeChangedHandler(mCommandToken);
      mCommandToken = 0;
      mCommandRegistry = nullptr;
    }
    mCommandTargets = nil;
    mGeneratedCommandItems = nil;
    mCommandMenuItem = nil;
    if (gCoralDockMenu == mDockMenu) gCoralDockMenu = nil;
    mDockMenu = nil;
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
    if (mPopover != nil) [mPopover close];
    mPopover = nil;
    mPopoverController = nil;
    if (mWindow != nil) {
      mWindow.toolbar = nil;
      mWindow.delegate = nil;
      [mWindow orderOut:nil];
      [mWindow close];
      mWindow = nil;
    }
    for (NSView* view in mNativeViews) [view removeFromSuperview];
    [mNativeViews removeAllObjects];
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
    if (mOptions.fullSizeContentView) style |= NSWindowStyleMaskFullSizeContentView;
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
    if (!mOptions.restorationIdentifier.empty()) {
      mWindow.restorable = YES;
      mWindow.identifier = (NSUserInterfaceItemIdentifier)
        [NSString stringWithUTF8String:mOptions.restorationIdentifier.c_str()];
      mWindow.restorationClass = [CoralWindowRestoration class];
    }
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
    ensureCommandMenu();
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
    // The Metal multi-list path reuses the open frame and has passed the UI/native suites; enable it
    // by default now that the paravirtual-GPU fallback and plan presentation are stable.
    return mSurface != nullptr;
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

  void ensureCommandMenu() {
    if (mCommandBound || mHost == nullptr) return;
    CommandRegistry* registry = mHost->commandRegistry();
    if (registry == nullptr) return;
    mCommandBound = true;
    mCommandRegistry = registry;
    mCommandToken = registry->addChangedHandler([this] { rebuildCommandMenu(); });
    rebuildCommandMenu();
  }

  void invokeCommand(const std::string_view name) {
    if (mHost == nullptr) return;
    if (CommandRegistry* registry = mHost->commandRegistry()) registry->invoke(std::string(name));
  }

  [[nodiscard]] bool commandEnabled(const std::string_view name) const {
    if (mHost == nullptr) return false;
    CommandRegistry* registry = mHost->commandRegistry();
    return registry != nullptr && registry->isEnabled(std::string(name));
  }

  [[nodiscard]] bool commandChecked(const std::string_view name) const {
    if (mHost == nullptr) return false;
    CommandRegistry* registry = mHost->commandRegistry();
    return registry != nullptr && registry->isChecked(std::string(name));
  }

  /** A registry group named after a standard menu is appended to that menu, not Commands. */
  NSMenu* standardMenuForGroup(NSString* group) {
    NSMenu* mainMenu = [NSApp mainMenu];
    if (mainMenu == nil || group.length == 0) return nil;
    if ([group isEqualToString:@"App"]) {
      return mainMenu.itemArray.firstObject.submenu;
    }
    for (NSMenuItem* item in mainMenu.itemArray) {
      if ([item.title isEqualToString:group]) return item.submenu;
    }
    return nil;
  }

  void rebuildCommandMenu() {
    if (mHost == nullptr) return;
    CommandRegistry* registry = mHost->commandRegistry();
    NSMenu* mainMenu = [NSApp mainMenu];
    if (registry == nullptr || mainMenu == nil) return;
    if (mCommandMenuItem != nil) {
      [mainMenu removeItem:mCommandMenuItem];
      mCommandMenuItem = nil;
    }
    for (NSMenuItem* item in mGeneratedCommandItems) {
      if (item.menu != nil) [item.menu removeItem:item];
    }
    mGeneratedCommandItems = [NSMutableArray array];
    mCommandTargets = [NSMutableArray array];
    NSMutableSet<NSMenu*>* separatedMenus = [NSMutableSet set];

    struct Group {
      NSString* name = nil;
      NSMutableArray<NSMenuItem*>* items = nil;
    };
    struct Entry {
      std::string name;
      CommandRegistry::Info info;
    };
    std::vector<Entry> entries;
    for (const std::string& name: registry->names()) {
      const std::optional<CommandRegistry::Info> info = registry->info(name);
      if (info.has_value() && !info->title.empty()) entries.push_back({ name, *info });
    }
    std::stable_sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
      if (left.info.order != right.info.order) return left.info.order < right.info.order;
      return left.name < right.name;
    });
    std::vector<Group> groups;
    for (const Entry& entry: entries) {
      const std::string& name = entry.name;
      const CommandRegistry::Info* info = &entry.info;
      NSString* groupName = info->group.empty()
        ? @"Commands" : [NSString stringWithUTF8String:info->group.c_str()];
      Group* group = nullptr;
      for (Group& candidate: groups) {
        if ([candidate.name isEqualToString:groupName]) { group = &candidate; break; }
      }
      if (group == nullptr) {
        Group created;
        created.name = groupName;
        created.items = [NSMutableArray array];
        groups.push_back(created);
        group = &groups.back();
      }
      CoralCommandTarget* target = [[CoralCommandTarget alloc] init];
      target.platform = this;
      target.commandName = [NSString stringWithUTF8String:name.c_str()];
      [mCommandTargets addObject:target];
      NSString* title = [NSString stringWithUTF8String:info->title.c_str()];
      const Shortcut shortcut = registry->shortcutFor(name);
      NSString* key = nil;
      NSEventModifierFlags modifiers = 0;
      if (shortcut.key >= Key::A && shortcut.key <= Key::Z) {
        key = [NSString stringWithFormat:@"%c", static_cast<char>('a' + (static_cast<int>(shortcut.key) - static_cast<int>(Key::A)))];
      } else if (shortcut.key >= Key::Digit0 && shortcut.key <= Key::Digit9) {
        key = [NSString stringWithFormat:@"%d", static_cast<int>(shortcut.key) - static_cast<int>(Key::Digit0)];
      }
      if (key != nil) {
        if (shortcut.meta) modifiers |= NSEventModifierFlagCommand;
        if (shortcut.control) modifiers |= NSEventModifierFlagControl;
        if (shortcut.alt) modifiers |= NSEventModifierFlagOption;
        if (shortcut.shift) modifiers |= NSEventModifierFlagShift;
      }
      NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
        action:@selector(invokeCommand:) keyEquivalent:key != nil ? key : @""];
      item.target = target;
      item.keyEquivalentModifierMask = modifiers;
      if (!info->icon.empty() && @available(macOS 11.0, *)) {
        item.image = [NSImage imageWithSystemSymbolName:
          [NSString stringWithUTF8String:info->icon.c_str()] accessibilityDescription:nil];
      }
      NSMenu* standard = standardMenuForGroup(groupName);
      if (standard != nil) {
        if (info->separatorBefore ||
            ![separatedMenus containsObject:standard]) {
          NSMenuItem* separator = [NSMenuItem separatorItem];
          const NSInteger separatorIndex = info->menuIndex >= 0
            ? std::min<NSInteger>(info->menuIndex, standard.numberOfItems)
            : standard.numberOfItems;
          [standard insertItem:separator atIndex:separatorIndex];
          [mGeneratedCommandItems addObject:separator];
          [separatedMenus addObject:standard];
        }
        const NSInteger index = info->menuIndex >= 0
          ? std::min<NSInteger>(info->menuIndex, standard.numberOfItems)
          : standard.numberOfItems;
        [standard insertItem:item atIndex:index];
        [mGeneratedCommandItems addObject:item];
      } else {
        [group->items addObject:item];
        [mGeneratedCommandItems addObject:item];
      }
    }

    NSMenu* commands = [[NSMenu alloc] initWithTitle:@"Commands"];
    bool hasCommands = false;
    for (Group& group: groups) {
      if (group.items.count == 0) continue;
      hasCommands = true;
      NSMenu* submenu = [[NSMenu alloc] initWithTitle:group.name];
      for (NSMenuItem* item in group.items) [submenu addItem:item];
      NSMenuItem* groupItem = [[NSMenuItem alloc] initWithTitle:group.name action:nil keyEquivalent:@""];
      groupItem.submenu = submenu;
      [commands addItem:groupItem];
    }
    if (hasCommands) {
      mCommandMenuItem = [[NSMenuItem alloc] initWithTitle:@"Commands" action:nil keyEquivalent:@""];
      mCommandMenuItem.submenu = commands;
      [mainMenu addItem:mCommandMenuItem];
    }

    // The Dock menu mirrors the registry's commands for the key window. It has no key equivalents:
    // the Dock is a pointer target and AppKit's menu bar owns the shortcuts.
    NSMenu* dock = [[NSMenu alloc] initWithTitle:@"Dock"];
    NSMutableDictionary<NSString*, NSMenu*>* dockGroups = [NSMutableDictionary dictionary];
    NSMutableArray<NSString*>* dockGroupOrder = [NSMutableArray array];
    for (const Entry& entry: entries) {
      CoralCommandTarget* target = [[CoralCommandTarget alloc] init];
      target.platform = this;
      target.commandName = [NSString stringWithUTF8String:entry.name.c_str()];
      [mCommandTargets addObject:target];
      NSString* title = [NSString stringWithUTF8String:entry.info.title.c_str()];
      NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
        action:@selector(invokeCommand:) keyEquivalent:@""];
      item.target = target;
      if (!entry.info.icon.empty() && @available(macOS 11.0, *)) {
        item.image = [NSImage imageWithSystemSymbolName:
          [NSString stringWithUTF8String:entry.info.icon.c_str()] accessibilityDescription:nil];
      }
      NSString* groupName = entry.info.group.empty()
        ? @"Commands" : [NSString stringWithUTF8String:entry.info.group.c_str()];
      NSMenu* groupMenu = dockGroups[groupName];
      if (groupMenu == nil) {
        groupMenu = [[NSMenu alloc] initWithTitle:groupName];
        dockGroups[groupName] = groupMenu;
        [dockGroupOrder addObject:groupName];
      }
      [groupMenu addItem:item];
    }
    for (NSString* groupName in dockGroupOrder) {
      NSMenu* groupMenu = dockGroups[groupName];
      NSMenuItem* groupItem = [[NSMenuItem alloc] initWithTitle:groupName
        action:nil keyEquivalent:@""];
      groupItem.submenu = groupMenu;
      [dock addItem:groupItem];
    }
    mDockMenu = dock.itemArray.count > 0 ? dock : nil;
    gCoralDockMenu = mDockMenu;
  }

  void setTitlebarAccessoryItems(std::vector<Window::TitlebarAccessoryItem> items) override {
    mTitlebarItems = std::move(items);
    applyTitlebarAccessory();
  }

  void setTitlebarAccessorySelectionHandler(std::function<void(int)> handler) override {
    mTitlebarAccessorySelection = std::move(handler);
  }

  void setTitlebarAccessoryView(void* nativeView) override {
    mTitlebarCustomView = nativeView;
    applyTitlebarAccessory();
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
    if (mTitlebarCustomView != nullptr) {
      NSView* view = (__bridge NSView*)mTitlebarCustomView;
      if (view != nil) {
        mTitlebarAccessory = [[NSTitlebarAccessoryViewController alloc] init];
        mTitlebarAccessory.view = view;
        mTitlebarAccessory.layoutAttribute = NSLayoutAttributeTrailing;
        [mWindow addTitlebarAccessoryViewController:mTitlebarAccessory];
      }
      return;
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

  void showNativeContextMenu(std::vector<Window::NativeMenuItem> items, const Point position,
      std::function<void(std::string_view)> handler) override {
    if (mView == nil || items.empty()) return;
    if (mNativeMenuTarget == nil) mNativeMenuTarget = [[CoralNativeMenuTarget alloc] init];
    mNativeMenuTarget.platform = this;
    mNativeContextAction = std::move(handler);
    NSMenu* menu = buildNativeMenu(items, mNativeMenuTarget);
    if (menu == nil) return;
    [menu popUpMenuPositioningItem:nil
                         atLocation:NSMakePoint(position.x, mView.bounds.size.height - position.y)
                             inView:mView];
    mNativeContextAction = {};
  }

  void nativeContextMenuSelected(const std::string_view identifier) {
    if (mNativeContextAction) mNativeContextAction(identifier);
  }

  NSMenu* buildNativeMenu(const std::vector<Window::NativeMenuItem>& items,
      CoralNativeMenuTarget* target) {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Coral"];
    for (const Window::NativeMenuItem& item: items) {
      if (item.separator) {
        [menu addItem:[NSMenuItem separatorItem]];
        continue;
      }
      NSString* title = item.title.empty() ? [NSString stringWithUTF8String:item.id.c_str()]
        : [NSString stringWithUTF8String:item.title.c_str()];
      NSMenuItem* native = [[NSMenuItem alloc] initWithTitle:title ?: @""
        action:@selector(select:) keyEquivalent:@""];
      native.target = target;
      native.representedObject = item.id.empty() ? nil
        : [NSString stringWithUTF8String:item.id.c_str()];
      native.enabled = item.enabled ? YES : NO;
      native.state = item.checked ? NSControlStateValueOn : NSControlStateValueOff;
      if (!item.children.empty()) {
        native.submenu = buildNativeMenu(item.children, target);
      }
      [menu addItem:native];
    }
    return menu;
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

  void setSidebarEditHandler(
      std::function<void(std::string_view, std::string_view)> handler) override {
    mSidebarEdit = std::move(handler);
  }

  void sidebarLabelEdited(const std::string_view identifier, const std::string_view label) {
    if (mSidebarEdit) mSidebarEdit(identifier, label);
  }

  void setSidebarMoveHandler(
      std::function<void(std::string_view, std::string_view, int)> handler) override {
    mSidebarMove = std::move(handler);
  }

  void setSidebarContextActions(std::vector<Window::SidebarContextAction> actions) override {
    mSidebarContextActions = std::move(actions);
    applySidebarContextActions();
  }

  void setSidebarContextActionHandler(
      std::function<void(std::string_view, std::string_view)> handler) override {
    mSidebarContextAction = std::move(handler);
  }

  void sidebarContextActionTriggered(const std::string_view itemId,
      const std::string_view actionId) {
    if (mSidebarContextAction) mSidebarContextAction(itemId, actionId);
  }

  void applySidebarContextActions() {
    if (mSidebarController == nil) return;
    NSMutableArray<CoralSidebarContextAction*>* actions =
      [NSMutableArray arrayWithCapacity:mSidebarContextActions.size()];
    for (const Window::SidebarContextAction& action: mSidebarContextActions) {
      CoralSidebarContextAction* target = [[CoralSidebarContextAction alloc] init];
      target.identifier = [NSString stringWithUTF8String:action.id.c_str()];
      target.title = [NSString stringWithUTF8String:action.title.c_str()];
      [actions addObject:target];
    }
    mSidebarController.contextActions = actions;
    [mSidebarController updateContextMenu];
  }

  bool moveSidebarItem(const std::string_view id, const std::string_view parentId,
      const int index) override {
    if (id.empty()) return false;
    const Window::SidebarItem* moved = findSidebarItem(mSidebarItems, id);
    if (moved == nullptr) return false;
    if (!parentId.empty() && findSidebarItem(mSidebarItems, parentId) == nullptr) return false;
    if (!parentId.empty() && containsSidebarId(*moved, parentId)) return false;
    Window::SidebarItem taken;
    if (!takeSidebarItem(mSidebarItems, id, taken)) return false;
    if (!insertSidebarItem(mSidebarItems, parentId, index, std::move(taken))) return false;
    applySidebar();
    if (mSidebarMove) mSidebarMove(id, parentId, index);
    return true;
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
    std::size_t generatedId = 0;
    std::function<CoralSidebarNode*(const Window::SidebarItem&)> makeNode =
      [&](const Window::SidebarItem& item) -> CoralSidebarNode* {
        CoralSidebarNode* node = [[CoralSidebarNode alloc] init];
        node.identifier = item.id.empty()
          ? [NSString stringWithFormat:@"coral.sidebar.%zu", generatedId++]
          : [NSString stringWithUTF8String:item.id.c_str()];
        node.label = item.label.empty()
          ? node.identifier : [NSString stringWithUTF8String:item.label.c_str()];
        node.symbol = item.symbol.empty() ? @"" :
          [NSString stringWithUTF8String:item.symbol.c_str()];
        node.group = item.group;
        node.editable = item.editable;
        NSMutableArray<CoralSidebarNode*>* children =
          [NSMutableArray arrayWithCapacity:item.children.size()];
        for (const Window::SidebarItem& child: item.children) {
          [children addObject:makeNode(child)];
        }
        node.children = children;
        return node;
      };
    NSMutableArray<CoralSidebarNode*>* roots =
      [NSMutableArray arrayWithCapacity:mSidebarItems.size()];
    for (const Window::SidebarItem& item: mSidebarItems) {
      [roots addObject:makeNode(item)];
    }
    mSidebarController.roots = roots;
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
      NSOutlineView* table = [[NSOutlineView alloc] initWithFrame:scroll.bounds];
      table.headerView = nil;
      table.rowSizeStyle = NSTableViewRowSizeStyleMedium;
      table.selectionHighlightStyle = NSTableViewSelectionHighlightStyleSourceList;
      table.allowsMultipleSelection = NO;
      table.indentationPerLevel = 14.0;
      NSTableColumn* column = [[NSTableColumn alloc] initWithIdentifier:@"coral.sidebar.column"];
      column.width = 220.0;
      [table addTableColumn:column];
      table.outlineTableColumn = column;
      [table registerForDraggedTypes:@[ @"com.coral.sidebar.item" ]];
      [table setDraggingSourceOperationMask:NSDragOperationMove forLocal:YES];
      [table setDraggingSourceOperationMask:NSDragOperationMove forLocal:NO];
      table.delegate = mSidebarController;
      table.dataSource = mSidebarController;
      mSidebarController.outline = table;
      scroll.documentView = table;
      [background addSubview:scroll];
      sidebar.view = background;
      mSidebarSplitItem = [NSSplitViewItem sidebarWithViewController:sidebar];
      mSidebarSplitItem.canCollapse = YES;
      mSidebarSplitItem.minimumThickness = 160.0;
      mSidebarSplitItem.maximumThickness = 320.0;
      [mSplitController insertSplitViewItem:mSidebarSplitItem atIndex:0];
    }
    // The first call creates the outline after roots were assigned; reload/expand once the view
    // exists, otherwise the initial tree is collapsed and numberOfRows only counts the roots.
    [mSidebarController.outline reloadData];
    [mSidebarController.outline expandItem:nil expandChildren:YES];
    mSidebarSplitItem.collapsed = !mSidebarVisible;
    applySidebarContextActions();
  }

  bool captureWindow(graphics::Bitmap* out) override {
    if (out == nullptr || mWindow == nil) return false;
    if (@available(macOS 14.0, *)) {
      __block SCShareableContent* content = nil;
      dispatch_semaphore_t contentReady = dispatch_semaphore_create(0);
      [SCShareableContent getShareableContentExcludingDesktopWindows:YES
        onScreenWindowsOnly:YES completionHandler:^(SCShareableContent* shareable, NSError* error) {
          (void)error;
          content = shareable;
          dispatch_semaphore_signal(contentReady);
        }];
      if (dispatch_semaphore_wait(contentReady,
          dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC)) != 0 || content == nil) {
        return false;
      }
      SCWindow* target = nil;
      for (SCWindow* window in content.windows) {
        if (window.windowID == static_cast<CGWindowID>(mWindow.windowNumber)) {
          target = window;
          break;
        }
      }
      if (target == nil) return false;
      SCContentFilter* filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:target];
      if (filter == nil) return false;
      SCStreamConfiguration* configuration = [[SCStreamConfiguration alloc] init];
      const CGFloat scale = std::max<CGFloat>(1.0, mWindow.backingScaleFactor);
      configuration.width = static_cast<size_t>(std::max<CGFloat>(1.0, target.frame.size.width) * scale);
      configuration.height = static_cast<size_t>(std::max<CGFloat>(1.0, target.frame.size.height) * scale);
      configuration.showsCursor = NO;
      configuration.capturesAudio = NO;
      configuration.captureResolution = SCCaptureResolutionBest;
      __block CGImageRef captured = nullptr;
      dispatch_semaphore_t imageReady = dispatch_semaphore_create(0);
      [SCScreenshotManager captureImageWithFilter:filter configuration:configuration
        completionHandler:^(CGImageRef image, NSError* error) {
          (void)error;
          if (image != nullptr) captured = CGImageRetain(image);
          dispatch_semaphore_signal(imageReady);
        }];
      if (dispatch_semaphore_wait(imageReady,
          dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC)) != 0 || captured == nullptr) {
        return false;
      }
      NSBitmapImageRep* representation = [[NSBitmapImageRep alloc] initWithCGImage:captured];
      CGImageRelease(captured);
      if (representation == nil) return false;
      NSData* png = [representation representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
      if (png == nil || png.length == 0) return false;
      std::string error;
      graphics::Image decoded = graphics::Image::decode(
        graphics::Data::makeCopy(png.bytes, static_cast<size_t>(png.length)), &error);
      if (decoded.isEmpty()) return false;
      *out = decoded.makeBitmap();
      return !out->isEmpty();
    }
    return false;
  }

  [[nodiscard]] bool supportsWindowCapture() const override {
    if (@available(macOS 14.0, *)) return true;
    return false;
  }

  NSImageView* makePrintView() {
    if (mSurface == nullptr || mView == nil) return nil;
    graphics::Bitmap bitmap;
    if (!mSurface->readback(&bitmap) || bitmap.isEmpty()) return nil;
    graphics::Image image = graphics::Image::fromBitmap(bitmap);
    std::string error;
    graphics::Data png = image.encode(graphics::ImageFormat::PNG, 100, &error);
    if (png.isEmpty()) return nil;
    NSData* data = [NSData dataWithBytes:png.bytes() length:png.size()];
    NSImage* nativeImage = [[NSImage alloc] initWithData:data];
    if (nativeImage == nil) return nil;
    NSImageView* view = [[NSImageView alloc] initWithFrame:NSMakeRect(0.0, 0.0,
      mWidth / std::max(1.0f, mDpiScale), mHeight / std::max(1.0f, mDpiScale))];
    view.image = nativeImage;
    view.imageScaling = NSImageScaleProportionallyUpOrDown;
    return view;
  }

  bool printWindow() override {
    NSImageView* view = makePrintView();
    if (view == nil) return false;
    NSPrintOperation* operation = [NSPrintOperation printOperationWithView:view
                                                               printInfo:[NSPrintInfo sharedPrintInfo]];
    [operation runOperation];
    return true;
  }

  bool printToFile(const std::string_view path) override {
    NSImageView* view = makePrintView();
    if (view == nil || path.empty()) return false;
    NSPrintInfo* info = [[NSPrintInfo sharedPrintInfo] copy];
    info.jobDisposition = NSPrintSaveJob;
    info.dictionary[NSPrintSavePath] = [NSString stringWithUTF8String:std::string(path).c_str()];
    NSPrintOperation* operation = [NSPrintOperation printOperationWithView:view printInfo:info];
    operation.showsPrintPanel = NO;
    operation.showsProgressPanel = NO;
    return [operation runOperation];
  }

  [[nodiscard]] bool supportsPrinting() const override {
    return true;
  }

  void setBackgroundMaterial(const Window::BackgroundMaterial material) override {
    if (mWindow == nil) return;
    if (material == Window::BackgroundMaterial::None) {
      mBackgroundEffect.hidden = YES;
      mMetalLayer.opaque = YES;
      if (mView.layer != nil) mView.layer.opaque = YES;
      mWindow.backgroundColor = [NSColor windowBackgroundColor];
      return;
    }
    ensureMaterialContainer();
    NSVisualEffectMaterial visual = NSVisualEffectMaterialUnderWindowBackground;
    switch (material) {
      case Window::BackgroundMaterial::Sidebar: visual = NSVisualEffectMaterialSidebar; break;
      case Window::BackgroundMaterial::HeaderView: visual = NSVisualEffectMaterialHeaderView; break;
      case Window::BackgroundMaterial::ContentBackground:
        visual = NSVisualEffectMaterialContentBackground; break;
      case Window::BackgroundMaterial::HUDWindow: visual = NSVisualEffectMaterialHUDWindow; break;
      case Window::BackgroundMaterial::UnderWindowBackground:
      case Window::BackgroundMaterial::None:
      default: visual = NSVisualEffectMaterialUnderWindowBackground; break;
    }
    mBackgroundEffect.material = visual;
    mBackgroundEffect.hidden = NO;
    mMetalLayer.opaque = NO;
    if (mView.layer != nil) mView.layer.opaque = NO;
    mWindow.backgroundColor = [NSColor clearColor];
  }

  void ensureMaterialContainer() {
    if (mMaterialController != nil) return;
    NSViewController* root = [[NSViewController alloc] init];
    NSView* container = [[NSView alloc] initWithFrame:mWindow.contentView.bounds];
    container.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    mBackgroundEffect = [[NSVisualEffectView alloc] initWithFrame:container.bounds];
    mBackgroundEffect.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    mBackgroundEffect.blendingMode = NSVisualEffectBlendingModeWithinWindow;
    mBackgroundEffect.state = NSVisualEffectStateActive;
    mBackgroundEffect.hidden = YES;
    [container addSubview:mBackgroundEffect];
    [root addChildViewController:mSplitController];
    mSplitController.view.frame = container.bounds;
    mSplitController.view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    if (mSplitController.view.layer != nil) {
      mSplitController.view.layer.backgroundColor = [NSColor clearColor].CGColor;
    }
    [container addSubview:mSplitController.view];
    root.view = container;
    mMaterialController = root;
    mWindow.contentViewController = root;
  }

  void showShareSheet(const std::string_view text, const Rect anchor) override {
    if (text.empty() || mView == nil) return;
    NSString* value = [NSString stringWithUTF8String:std::string(text).c_str()];
    if (value.length == 0) return;
    NSSharingServicePicker* picker = [[NSSharingServicePicker alloc] initWithItems:@[ value ]];
    NSRect rect = NSMakeRect(mView.bounds.size.width * 0.5, mView.bounds.size.height * 0.5, 1.0, 1.0);
    if (anchor.getWidth() > 0.0f && anchor.getHeight() > 0.0f) {
      rect = NSMakeRect(anchor.getLeft(), mView.bounds.size.height - anchor.getBottom(),
        anchor.getWidth(), anchor.getHeight());
    }
    mSharePicker = picker;
    [picker showRelativeToRect:rect ofView:mView preferredEdge:NSRectEdgeMinY];
  }

  [[nodiscard]] bool supportsShareSheet() const override {
    return true;
  }

  void setContentFocusRingEnabled(const bool enabled) override {
    if (mView == nil) return;
    mView.focusRingType = enabled ? NSFocusRingTypeExterior : NSFocusRingTypeDefault;
    [mView setKeyboardFocusRingNeedsDisplayInRect:mView.bounds];
  }

  void setDockBadge(const std::string_view text) override {
    NSString* badge = text.empty() ? nil : [NSString stringWithUTF8String:text.data()];
    NSApp.dockTile.badgeLabel = badge;
  }

  void requestUserAttention(const bool critical) override {
    [NSApp requestUserAttention:critical ? NSCriticalRequest : NSInformationalRequest];
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

  void setToolbarStyle(const Window::ToolbarStyle style) override {
    mOptions.toolbarStyle = style;
    applyToolbarStyle();
  }

  void setTitlebarVisible(const bool visible) override {
    mOptions.titlebarVisible = visible;
    if (mWindow != nil) mWindow.titleVisibility = visible ? NSWindowTitleVisible : NSWindowTitleHidden;
    notifySafeAreaChanged();
  }

  void setWindowButtonsVisible(const bool visible) override {
    mOptions.windowButtonsVisible = visible;
    if (mWindow == nil) return;
    for (NSWindowButton button: { NSWindowCloseButton, NSWindowMiniaturizeButton,
             NSWindowZoomButton }) {
      [mWindow standardWindowButton:button].hidden = visible ? NO : YES;
    }
  }

  void showNativePopover(void* nativeView, const Rect anchor) override {
    if (mView == nil || nativeView == nullptr) return;
    NSView* content = (__bridge NSView*)nativeView;
    if (![content isKindOfClass:[NSView class]]) return;
    if (mPopover == nil) {
      mPopover = [[NSPopover alloc] init];
      mPopover.behavior = NSPopoverBehaviorTransient;
    }
    NSViewController* controller = [[NSViewController alloc] init];
    controller.view = content;
    mPopover.contentViewController = controller;
    mPopoverController = controller;
    const NSRect rect = NSMakeRect(anchor.getLeft(),
      mView.bounds.size.height - anchor.getBottom(), std::max(1.0f, anchor.getWidth()),
      std::max(1.0f, anchor.getHeight()));
    [mPopover showRelativeToRect:rect ofView:mView preferredEdge:NSRectEdgeMinY];
  }

  void hideNativePopover() override {
    if (mPopover != nil) [mPopover close];
    mPopover = nil;
    mPopoverController = nil;
  }

  [[nodiscard]] bool isNativePopoverVisible() const override {
    return mPopover != nil && mPopover.isShown;
  }

  [[nodiscard]] bool supportsNativePopover() const override {
    return true;
  }

  void setNativeViewBounds(void* nativeView, const Rect bounds, const bool visible) override {
    if (mView == nil || nativeView == nullptr) return;
    NSView* view = (__bridge NSView*)nativeView;
    if (![view isKindOfClass:[NSView class]]) return;
    if (![mNativeViews containsObject:view]) {
      [mView addSubview:view];
      [mNativeViews addObject:view];
    }
    view.hidden = visible ? NO : YES;
    view.frame = NSMakeRect(bounds.getLeft(),
      mView.bounds.size.height - bounds.getBottom(),
      std::max(0.0f, bounds.getWidth()), std::max(0.0f, bounds.getHeight()));
  }

  void removeNativeView(void* nativeView) override {
    if (nativeView == nullptr) return;
    NSView* view = (__bridge NSView*)nativeView;
    if (![view isKindOfClass:[NSView class]]) return;
    NSResponder* first = mWindow.firstResponder;
    if (mView != nil && first != nil && [first isKindOfClass:[NSView class]] &&
        [(NSView*)first isDescendantOf:view]) {
      [mWindow makeFirstResponder:mView];
    }
    [view removeFromSuperview];
    [mNativeViews removeObject:view];
  }

  void applyToolbarStyle() {
    if (mWindow == nil) return;
    if (@available(macOS 11.0, *)) {
      switch (mOptions.toolbarStyle) {
        case Window::ToolbarStyle::Expanded: mWindow.toolbarStyle = NSWindowToolbarStyleExpanded; break;
        case Window::ToolbarStyle::Preference: mWindow.toolbarStyle = NSWindowToolbarStylePreference; break;
        case Window::ToolbarStyle::Unified: mWindow.toolbarStyle = NSWindowToolbarStyleUnified; break;
        case Window::ToolbarStyle::UnifiedCompact:
          mWindow.toolbarStyle = NSWindowToolbarStyleUnifiedCompact; break;
        case Window::ToolbarStyle::Automatic:
        default: mWindow.toolbarStyle = NSWindowToolbarStyleUnified; break;
      }
    }
    refreshContentLayout();
    updateBackingSize(true);
    notifySafeAreaChanged();
  }

  void setToolbarItems(std::vector<Window::ToolbarItem> items) override {
    mToolbarItems = std::move(items);
    applyToolbar();
  }

  void setToolbarVisible(const bool visible) override {
    mToolbarVisible = visible;
    if (mWindow != nil) {
      if (!visible) {
        mWindow.toolbar = nil;
        mFullscreenToolbarHidden = true;
      } else if (mFullscreen && mOptions.fullscreenToolbarAutoHide) {
        mToolbar.visible = NO;
        mFullscreenToolbarHidden = true;
      } else {
        mWindow.toolbar = mToolbar;
        mFullscreenToolbarHidden = false;
      }
    }
    refreshContentLayout();
    updateBackingSize(true);
    notifySafeAreaChanged();
  }

  void setToolbarAutoHideInFullScreen(const bool enabled) override {
    mOptions.fullscreenToolbarAutoHide = enabled;
    if (mWindow == nil) return;
    if (mFullscreen) {
      if (enabled) {
        hideFullscreenToolbar();
      } else {
        showFullscreenToolbar();
      }
    } else {
      mFullscreenToolbarHidden = false;
      mWindow.toolbar = mToolbarVisible ? mToolbar : nil;
    }
    refreshContentLayout();
    updateBackingSize(true);
  }

  void notifySafeAreaChanged() {
    if (mHost != nullptr) mHost->platformSafeAreaChanged(safeAreaInsets());
  }

  /** Hides the toolbar while full screen (the pointer leaves the top edge). */
  void hideFullscreenToolbar() {
    if (mWindow == nil || mToolbar == nil || !mToolbarVisible) return;
    if (mFullscreenToolbarHidden && mToolbar.visible == NO) return;
    mFullscreenToolbarHidden = true;
    mToolbar.visible = NO;
    refreshContentLayout();
    updateBackingSize(true);
    if (mHost != nullptr) mHost->platformResized(mWidth, mHeight);
    notifySafeAreaChanged();
  }

  /** Reveals the toolbar when the pointer reaches the top edge in full screen. */
  void showFullscreenToolbar() {
    if (mWindow == nil || mToolbar == nil || !mToolbarVisible) return;
    if (!mFullscreenToolbarHidden && mToolbar.visible) return;
    mFullscreenToolbarHidden = false;
    if (mWindow.toolbar != mToolbar) mWindow.toolbar = mToolbar;
    mToolbar.visible = YES;
    refreshContentLayout();
    updateBackingSize(true);
    if (mHost != nullptr) mHost->platformResized(mWidth, mHeight);
    notifySafeAreaChanged();
  }

  void fullscreenMouseExited() {
    if (mWindow == nil || !mFullscreen || !mOptions.fullscreenToolbarAutoHide) return;
    hideFullscreenToolbar();
  }

  void fullscreenMouseMoved(const NSPoint locationInWindow) {
    if (mWindow == nil || !mFullscreen || !mOptions.fullscreenToolbarAutoHide) return;
    if (!mToolbarVisible || mToolbar == nil) return;
    const CGFloat top = mWindow.frame.size.height;
    if (locationInWindow.y >= top - 8.0) {
      showFullscreenToolbar();
    } else if (!mFullscreenToolbarHidden) {
      hideFullscreenToolbar();
    }
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
      if (item.flexibleSpace) {
        NSToolbarItem* nativeItem = [[NSToolbarItem alloc]
          initWithItemIdentifier:NSToolbarFlexibleSpaceItemIdentifier];
        items[NSToolbarFlexibleSpaceItemIdentifier] = nativeItem;
        [identifiers addObject:NSToolbarFlexibleSpaceItemIdentifier];
        continue;
      }
      if (item.separator) {
        // AppKit's standard separator can be merged away in unified toolbars. A custom view keeps a
        // deliberate 1px divider and still participates in item overflow.
        NSToolbarItem* nativeItem = [[NSToolbarItem alloc] initWithItemIdentifier:identifier];
        const CGFloat height = mWindow.toolbar != nil ? 22.0 : 24.0;
        CoralToolbarSeparatorView* separator = [[CoralToolbarSeparatorView alloc]
          initWithFrame:NSMakeRect(0, 0, 9, height)];
        nativeItem.view = separator;
        nativeItem.label = @"";
        nativeItem.paletteLabel = @"";
        items[identifier] = nativeItem;
        [identifiers addObject:identifier];
        continue;
      }
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
      if (item.nativeView != nullptr) {
        NSView* customView = (__bridge NSView*)item.nativeView;
        if ([customView isKindOfClass:[NSView class]]) nativeItem.view = customView;
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
    applyToolbarStyle();
    if (mToolbarVisible && !(mFullscreen && mOptions.fullscreenToolbarAutoHide)) {
      mWindow.toolbar = mToolbar;
      mToolbar.visible = YES;
    }
    refreshContentLayout();
    updateBackingSize(true);
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
    if (mOptions.fullSizeContentView) desired |= NSWindowStyleMaskFullSizeContentView;
    if (mWindow.styleMask != desired) {
      mWindow.styleMask = desired;
    }
    mWindow.titleVisibility = mOptions.titlebarVisible ? NSWindowTitleVisible : NSWindowTitleHidden;
    mWindow.titlebarAppearsTransparent = mOptions.fullSizeContentView ? YES : NO;
    for (NSWindowButton button: { NSWindowCloseButton, NSWindowMiniaturizeButton,
             NSWindowZoomButton }) {
      [mWindow standardWindowButton:button].hidden = mOptions.windowButtonsVisible ? NO : YES;
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
    if (@available(macOS 10.14, *)) {
      appearance.accentColor = argbFromColor([NSColor controlAccentColor]);
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
    // CoralGraphics creates the CADisplayLink on the main thread and attaches it to its own run-loop
    // thread, so the content view can select the window's actual screen.
    return graphics::gpu::NativeDisplay{ graphics::gpu::NativeDisplayKind::MacOS, nullptr,
      (__bridge void*) mView };
  }

  [[nodiscard]] void* nativeWindowHandle() const override {
    return (__bridge void*) mWindow;
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

  [[nodiscard]] Insets safeAreaInsets() const override {
    if (mWindow == nil || mWindow.contentView == nil) return {};
    if (!mOptions.fullSizeContentView && !mFullscreen) return {};
    const NSRect content = [mWindow.contentView convertRect:mWindow.contentView.bounds toView:nil];
    const NSRect layout = mWindow.contentLayoutRect;
    const CGFloat top = std::max<CGFloat>(0.0, NSMaxY(content) - NSMaxY(layout));
    return Insets{ 0.0f, static_cast<float>(top), 0.0f, 0.0f };
  }

  [[nodiscard]] bool outputIsHdr() const override {
    if (mSurface == nullptr) return false;
    const backend::SwapchainColorSpace space = mSurface->displayColorSpace();
    return space == backend::SwapchainColorSpace::Hdr10Pq ||
      space == backend::SwapchainColorSpace::ExtendedLinearP3;
  }

  [[nodiscard]] OutputColorSpace outputColorSpace() const override {
    if (mSurface == nullptr) return mOptions.outputColorSpace;
    switch (mSurface->displayColorSpace()) {
      case backend::SwapchainColorSpace::DisplayP3: return OutputColorSpace::DisplayP3;
      case backend::SwapchainColorSpace::ExtendedLinearP3:
        return OutputColorSpace::ExtendedLinearP3;
      case backend::SwapchainColorSpace::Hdr10Pq: return OutputColorSpace::Hdr10Pq;
      case backend::SwapchainColorSpace::Srgb:
      default: return OutputColorSpace::Srgb;
    }
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
    // A FlagsChanged event is not a key event: -isARepeat and -characters raise for it. Build the
    // modifier state directly instead of routing it through keyEvent().
    KeyEvent translated;
    translated.key = key;
    const NSEventModifierFlags flags = event.modifierFlags;
    translated.shift = (flags & NSEventModifierFlagShift) != 0;
    translated.control = (flags & NSEventModifierFlagControl) != 0;
    translated.alt = (flags & NSEventModifierFlagOption) != 0;
    translated.meta = (flags & NSEventModifierFlagCommand) != 0;
    mHost->dispatchKey(translated, down);
  }

  /** Called by the window delegate when AppKit changed the content size or backing scale. */
  void onFocusChanged(Layer* focused) override {
    (void)focused;
    if (mView != nil) [mView.inputContext invalidateCharacterCoordinates];
  }

  /** Lays out the AppKit content hierarchy before sampling the Metal view size. */
  void refreshContentLayout() {
    if (mWindow == nil) return;
    [mWindow.contentView layoutSubtreeIfNeeded];
    if (mSplitController != nil) [mSplitController.view layoutSubtreeIfNeeded];
    if (mView != nil) [mView layoutSubtreeIfNeeded];
  }

  void windowDidChangeGeometry() {
    if (mWindow == nil) return;
    refreshContentLayout();
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
    const bool wasFullscreen = mFullscreen;
    mMaximized = [mWindow isZoomed] != NO;
    mMinimized = [mWindow isMiniaturized] != NO;
    mFullscreen = (mWindow.styleMask & NSWindowStyleMaskFullScreen) != 0;
    if (mFullscreen && !wasFullscreen) {
      mFullscreenToolbarHidden = true;
      if (mOptions.fullscreenToolbarAutoHide && mToolbarVisible) mToolbar.visible = NO;
    } else if (!mFullscreen && wasFullscreen) {
      mFullscreenToolbarHidden = false;
      if (mToolbarVisible && mToolbar != nil) mToolbar.visible = YES;
    }
    refreshContentLayout();
    if (mHost != nullptr) {
      mHost->platformStateChanged(mMaximized, mMinimized, mFullscreen);
    }
    updateBackingSize(true);
    notifySafeAreaChanged();
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

  /**
   * True when the current input source is a plain keyboard layout (ABC, U.S., ...) rather than an
   * input method (Pinyin, Kotoeri, ...).
   *
   * A keyboard layout does not need `interpretKeyEvents:` to compose text; dispatching the event's
   * characters directly keeps AppKit's InputMethodKit lazy. That is what avoids the one-time TSM /
   * IMKCFRunLoopWakeUpReliable diagnostics on the first plain-text key. An IME still takes the
   * AppKit path, so Chinese/Japanese composition is unchanged.
   */
  [[nodiscard]] bool usesKeyboardLayout() const {
    TISInputSourceRef source = TISCopyCurrentKeyboardInputSource();
    if (source == nullptr) return false;
    CFTypeRef type = TISGetInputSourceProperty(source, kTISPropertyInputSourceType);
    const bool layout = type != nullptr && CFEqual(type, kTISTypeKeyboardLayout);
    CFRelease(source);
    return layout;
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
    switch (mOptions.outputColorSpace) {
      case OutputColorSpace::DisplayP3:
        configuration.outputColorSpace = backend::SwapchainColorSpace::DisplayP3; break;
      case OutputColorSpace::ExtendedLinearP3:
        configuration.outputColorSpace = backend::SwapchainColorSpace::ExtendedLinearP3; break;
      case OutputColorSpace::Hdr10Pq:
        configuration.outputColorSpace = backend::SwapchainColorSpace::Hdr10Pq; break;
      case OutputColorSpace::Srgb:
      default:
        if (mOptions.hdrOutput) configuration.hdrOutput = true;
        break;
    }
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
    applicationItem.title = appName;
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
    fileItem.title = @"File";
    NSMenu* fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    NSMenuItem* close = [fileMenu addItemWithTitle:@"Close"
                                            action:@selector(performClose:)
                                     keyEquivalent:@"w"];
    close.keyEquivalentModifierMask = NSEventModifierFlagCommand;
    fileItem.submenu = fileMenu;
    [menuBar addItem:fileItem];

    // ---- Edit menu ----
    NSMenuItem* editItem = [[NSMenuItem alloc] init];
    editItem.title = @"Edit";
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

    // ---- View menu ----
    NSMenuItem* viewItem = [[NSMenuItem alloc] init];
    viewItem.title = @"View";
    NSMenu* viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    [viewMenu addItemWithTitle:@"Enter Full Screen"
                        action:@selector(toggleFullScreen:)
                 keyEquivalent:@"f"];
    viewMenu.itemArray.lastObject.keyEquivalentModifierMask =
      NSEventModifierFlagCommand | NSEventModifierFlagControl;
    viewItem.submenu = viewMenu;
    [menuBar addItem:viewItem];

    // ---- Window menu ----
    NSMenuItem* windowItem = [[NSMenuItem alloc] init];
    windowItem.title = @"Window";
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

  static const Window::SidebarItem* findSidebarItem(
      const std::vector<Window::SidebarItem>& nodes, const std::string_view id) {
    for (const Window::SidebarItem& item: nodes) {
      if (item.id == id) return &item;
      if (const Window::SidebarItem* nested = findSidebarItem(item.children, id)) return nested;
    }
    return nullptr;
  }

  static bool containsSidebarId(const Window::SidebarItem& item, const std::string_view id) {
    if (item.id == id) return true;
    for (const Window::SidebarItem& child: item.children) {
      if (containsSidebarId(child, id)) return true;
    }
    return false;
  }

  static bool takeSidebarItem(std::vector<Window::SidebarItem>& nodes, const std::string_view id,
      Window::SidebarItem& out) {
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
      if (it->id == id) {
        out = std::move(*it);
        nodes.erase(it);
        return true;
      }
      if (takeSidebarItem(it->children, id, out)) return true;
    }
    return false;
  }

  static bool insertSidebarItem(std::vector<Window::SidebarItem>& nodes,
      const std::string_view parentId, int index, Window::SidebarItem item) {
    if (parentId.empty()) {
      const int clamped = std::clamp(index, 0, static_cast<int>(nodes.size()));
      nodes.insert(nodes.begin() + clamped, std::move(item));
      return true;
    }
    for (Window::SidebarItem& node: nodes) {
      if (node.id == parentId) {
        const int clamped = std::clamp(index, 0, static_cast<int>(node.children.size()));
        node.children.insert(node.children.begin() + clamped, std::move(item));
        return true;
      }
      if (insertSidebarItem(node.children, parentId, index, item)) return true;
    }
    return false;
  }

  [[nodiscard]] bool canClose() const {
    return mHost != nullptr && mHost->canClose();
  }

  WindowHost* mHost = nullptr;
  CoralDragSource* mActiveDragSource = nil;
  NSSharingServicePicker* mSharePicker = nil;
  NSViewController* mMaterialController = nil;
  NSVisualEffectView* mBackgroundEffect = nil;
  NSToolbar* mToolbar = nil;
  CoralToolbarDelegate* mToolbarDelegate = nil;
  std::vector<Window::ToolbarItem> mToolbarItems;
  bool mFullscreenToolbarHidden = false;
  std::function<void(std::string_view)> mToolbarAction;
  bool mToolbarVisible = true;
  bool mSecureInputEnabled = false;
  CommandRegistry* mCommandRegistry = nullptr;
  int mCommandToken = 0;
  bool mCommandBound = false;
  NSMenuItem* mCommandMenuItem = nil;
  NSMenu* mDockMenu = nil;
  NSMutableArray<NSMenuItem*>* mGeneratedCommandItems = nil;
  NSMutableArray<CoralCommandTarget*>* mCommandTargets = nil;
  std::vector<Window::TitlebarAccessoryItem> mTitlebarItems;
  NSTitlebarAccessoryViewController* mTitlebarAccessory = nil;
  void* mTitlebarCustomView = nullptr;
  CoralNativeMenuTarget* mNativeMenuTarget = nil;
  NSPopover* mPopover = nil;
  NSViewController* mPopoverController = nil;
  NSMutableArray<NSView*>* mNativeViews = [NSMutableArray array];
  std::function<void(std::string_view)> mNativeContextAction;
  NSSegmentedControl* mTitlebarControl = nil;
  CoralTitlebarAccessoryTarget* mTitlebarTarget = nil;
  std::function<void(int)> mTitlebarAccessorySelection;
  std::vector<Window::SidebarItem> mSidebarItems;
  CoralSidebarController* mSidebarController = nil;
  NSSplitViewItem* mSidebarSplitItem = nil;
  CoralSplitViewController* mSplitController = nil;
  std::function<void(std::string_view)> mSidebarSelection;
  std::function<void(std::string_view, std::string_view)> mSidebarEdit;
  std::function<void(std::string_view, std::string_view, int)> mSidebarMove;
  std::vector<Window::SidebarContextAction> mSidebarContextActions;
  std::function<void(std::string_view, std::string_view)> mSidebarContextAction;
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

- (NSMenu*)applicationDockMenu:(NSApplication*)sender {
  (void)sender;
  return gCoralDockMenu;
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

@implementation CoralCommandTarget

@synthesize platform = _platform;
@synthesize commandName = _commandName;

- (void)invokeCommand:(id)sender {
  (void)sender;
  if (_platform == nullptr || _commandName == nil) return;
  static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->invokeCommand(
    _commandName.UTF8String != nullptr ? std::string_view(_commandName.UTF8String) : std::string_view());
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
  if (_platform == nullptr || _commandName == nil) return NO;
  auto* platform = static_cast<coral::ui::detail::CocoaPlatform*>(_platform);
  const std::string_view name = _commandName.UTF8String != nullptr
    ? std::string_view(_commandName.UTF8String) : std::string_view();
  item.state = platform->commandChecked(name) ? NSControlStateValueOn : NSControlStateValueOff;
  return platform->commandEnabled(name) ? YES : NO;
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

@implementation CoralSidebarNode
@end

@implementation CoralSidebarContextAction
@end

@implementation CoralSidebarController

@synthesize platform = _platform;
@synthesize roots = _roots;
@synthesize outline = _outline;
@synthesize contextActions = _contextActions;

- (void)updateContextMenu {
  if (_outline == nil) return;
  if (_contextActions.count == 0) {
    _outline.menu = nil;
    return;
  }
  if (_outline.menu == nil) {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Sidebar"];
    menu.delegate = self;
    _outline.menu = menu;
  }
  // Populate now as well: `menuNeedsUpdate:` otherwise only runs when the menu is about to open,
  // which leaves an empty item array for callers and for the native smoke.
  [self menuNeedsUpdate:_outline.menu];
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
  [menu removeAllItems];
  for (CoralSidebarContextAction* action in _contextActions) {
    NSMenuItem* item = [menu addItemWithTitle:action.title ?: @""
                                        action:@selector(contextAction:)
                                 keyEquivalent:@""];
    item.target = self;
    item.representedObject = action.identifier;
  }
}

- (void)contextAction:(NSMenuItem*)item {
  if (_platform == nullptr || _outline == nil) return;
  NSInteger row = _outline.clickedRow;
  if (row < 0) row = _outline.selectedRow;
  CoralSidebarNode* node = row >= 0 ? [_outline itemAtRow:row] : nil;
  NSString* identifier = node != nil ? node.identifier : nil;
  NSString* action = [item.representedObject isKindOfClass:[NSString class]]
    ? (NSString*)item.representedObject : nil;
  if (identifier.length == 0 || action.length == 0) return;
  static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->sidebarContextActionTriggered(
    identifier.UTF8String, action.UTF8String);
}

- (NSInteger)outlineView:(NSOutlineView*)outlineView numberOfChildrenOfItem:(id)item {
  (void)outlineView;
  NSArray<CoralSidebarNode*>* children = item != nil
    ? ((CoralSidebarNode*)item).children : _roots;
  return static_cast<NSInteger>(children.count);
}

- (id)outlineView:(NSOutlineView*)outlineView child:(NSInteger)index ofItem:(id)item {
  (void)outlineView;
  NSArray<CoralSidebarNode*>* children = item != nil
    ? ((CoralSidebarNode*)item).children : _roots;
  if (index < 0 || index >= static_cast<NSInteger>(children.count)) return nil;
  return children[static_cast<NSUInteger>(index)];
}

- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item {
  (void)outlineView;
  return item != nil && ((CoralSidebarNode*)item).children.count > 0;
}

- (NSView*)outlineView:(NSOutlineView*)outlineView viewForTableColumn:(NSTableColumn*)column
    item:(id)item {
  (void)column;
  CoralSidebarNode* node = (CoralSidebarNode*)item;
  if (node == nil) return nil;
  NSString* identifier = @"coral.sidebar.cell";
  NSTableCellView* cell = [outlineView makeViewWithIdentifier:identifier owner:self];
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
  cell.textField.stringValue = node.label ?: @"";
  cell.textField.editable = node.editable;
  cell.textField.selectable = node.editable;
  cell.textField.font = node.group ? [NSFont boldSystemFontOfSize:11.0]
                                   : [NSFont systemFontOfSize:13.0];
  if (!node.group && node.symbol.length > 0 && @available(macOS 11.0, *)) {
    cell.imageView.image = [NSImage imageWithSystemSymbolName:node.symbol
                                       accessibilityDescription:nil];
  } else {
    cell.imageView.image = nil;
  }
  return cell;
}

- (void)outlineView:(NSOutlineView*)outlineView setObjectValue:(id)object
    forTableColumn:(NSTableColumn*)column item:(id)item {
  (void)outlineView;
  (void)column;
  CoralSidebarNode* node = (CoralSidebarNode*)item;
  if (node == nil || !node.editable || _platform == nullptr) return;
  NSString* value = [object isKindOfClass:[NSString class]] ? (NSString*)object
    : [object respondsToSelector:@selector(stringValue)] ? [object stringValue] : nil;
  if (value == nil) return;
  node.label = value;
  const char* identifier = node.identifier.UTF8String;
  const char* label = value.UTF8String;
  if (identifier != nullptr && label != nullptr) {
    static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->sidebarLabelEdited(
      identifier, label);
  }
}

- (NSPasteboardItem*)outlineView:(NSOutlineView*)outlineView
    pasteboardWriterForItem:(id)item {
  (void)outlineView;
  CoralSidebarNode* node = (CoralSidebarNode*)item;
  if (node == nil || node.identifier.length == 0) return nil;
  NSPasteboardItem* pasteboard = [[NSPasteboardItem alloc] init];
  [pasteboard setString:node.identifier forType:@"com.coral.sidebar.item"];
  return pasteboard;
}

- (NSDragOperation)outlineView:(NSOutlineView*)outlineView validateDrop:(id<NSDraggingInfo>)info
    proposedItem:(id)item proposedChildIndex:(NSInteger)index {
  (void)outlineView;
  (void)item;
  (void)index;
  NSString* identifier = [[info draggingPasteboard] stringForType:@"com.coral.sidebar.item"];
  return identifier.length > 0 ? NSDragOperationMove : NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView acceptDrop:(id<NSDraggingInfo>)info
    item:(id)item childIndex:(NSInteger)index {
  if (_platform == nullptr) return NO;
  NSString* identifier = [[info draggingPasteboard] stringForType:@"com.coral.sidebar.item"];
  CoralSidebarNode* parent = (CoralSidebarNode*)item;
  NSString* parentId = parent != nil ? parent.identifier : @"";
  NSInteger targetIndex = index;
  if (targetIndex == NSOutlineViewDropOnItemIndex) {
    targetIndex = [self outlineView:outlineView numberOfChildrenOfItem:item];
  }
  if (identifier.length == 0 || parentId == nil) return NO;
  const char* id = identifier.UTF8String;
  const char* parentUtf8 = parentId.UTF8String;
  if (id == nullptr || parentUtf8 == nullptr) return NO;
  return static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->moveSidebarItem(
    id, parentUtf8, static_cast<int>(targetIndex)) ? YES : NO;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView isGroupItem:(id)item {
  (void)outlineView;
  return item != nil && ((CoralSidebarNode*)item).group;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView shouldSelectItem:(id)item {
  (void)outlineView;
  return item != nil && !((CoralSidebarNode*)item).group;
}

// Keep the table-view alias too: a caller that only knows NSTableView can still ask about a row.
- (BOOL)tableView:(NSTableView*)tableView isGroupRow:(NSInteger)row {
  (void)tableView;
  id item = [_outline itemAtRow:row];
  return item != nil && ((CoralSidebarNode*)item).group;
}

- (void)outlineViewSelectionDidChange:(NSNotification*)notification {
  (void)notification;
  if (_platform == nullptr || _outline == nil) return;
  const NSInteger row = _outline.selectedRow;
  CoralSidebarNode* node = row >= 0 ? [_outline itemAtRow:row] : nil;
  if (node == nil || node.identifier == nil) return;
  const char* identifier = node.identifier.UTF8String;
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

@implementation CoralNativeMenuTarget

@synthesize platform = _platform;

- (void)select:(NSMenuItem*)item {
  if (_platform == nullptr || item.representedObject == nil) return;
  NSString* identifier = [item.representedObject isKindOfClass:[NSString class]]
    ? (NSString*)item.representedObject : nil;
  if (identifier.length == 0) return;
  static_cast<coral::ui::detail::CocoaPlatform*>(_platform)->nativeContextMenuSelected(
    identifier.UTF8String);
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
    case coral::ui::AccessibilityRole::ComboBox: return NSAccessibilityComboBoxRole;
    case coral::ui::AccessibilityRole::ColorPicker: return NSAccessibilityColorWellRole;
    case coral::ui::AccessibilityRole::SpinButton: return NSAccessibilitySliderRole;
    case coral::ui::AccessibilityRole::Calendar: return NSAccessibilityGroupRole;
    case coral::ui::AccessibilityRole::ProgressIndicator:
      return NSAccessibilityProgressIndicatorRole;
    case coral::ui::AccessibilityRole::TabList: return NSAccessibilityTabGroupRole;
    case coral::ui::AccessibilityRole::TextField: return NSAccessibilityTextFieldRole;
    case coral::ui::AccessibilityRole::Label: return NSAccessibilityStaticTextRole;
    case coral::ui::AccessibilityRole::Link: return NSAccessibilityLinkRole;
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
  if (coral::ui::hasAccessibilityAction(node.actions, coral::ui::AccessibilityAction::Confirm)) {
    [actions addObject:NSAccessibilityConfirmAction];
  }
  if (coral::ui::hasAccessibilityAction(node.actions, coral::ui::AccessibilityAction::Cancel)) {
    [actions addObject:NSAccessibilityCancelAction];
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
  } else if ([action isEqualToString:NSAccessibilityConfirmAction]) {
    (void)platform->performAccessibilityAction(path, coral::ui::AccessibilityAction::Confirm, {});
  } else if ([action isEqualToString:NSAccessibilityCancelAction]) {
    (void)platform->performAccessibilityAction(path, coral::ui::AccessibilityAction::Cancel, {});
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

- (void)drawFocusRingMask {
  NSRectFill(self.bounds);
}

- (NSRect)focusRingMaskBounds {
  return self.bounds;
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
  if (platform != nullptr) {
    platform->fullscreenMouseMoved(event.locationInWindow);
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::None, 0, true);
  }
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
  if (platform != nullptr) {
    platform->fullscreenMouseExited();
    platform->dispatchPointerEvent(event, coral::ui::PointerButton::None, 0, false);
  }
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
    if (platform->usesKeyboardLayout()) {
      // No IME composition is needed for a keyboard layout. Dispatch the characters directly and
      // avoid initialising InputMethodKit just for a plain ASCII key.
      platform->dispatchKeyEvent(event, true);
      platform->dispatchTextEvent(event);
    } else {
      [self interpretKeyEvents:@[ event ]];
    }
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
  if (!coral::ui::detail::cocoaAccessibilityEnabled()) return @[];
  auto* platform = static_cast<CocoaPlatform*>(self.platform);
  if (platform == nullptr) return @[];
  const coral::ui::AccessibilityNode snapshot = platform->accessibilitySnapshot();
  if (snapshot.name.empty() && snapshot.description.empty() && snapshot.children.empty()) return @[];
  const NSRect logical = NSMakeRect(0.0, 0.0, self.bounds.size.width, self.bounds.size.height);
  return @[ makeCoralAXElement(snapshot, logical, self, self) ];
}

- (id)accessibilityHitTest:(NSPoint)point {
  if (!coral::ui::detail::cocoaAccessibilityEnabled()) return nil;
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
  if (!coral::ui::detail::cocoaAccessibilityEnabled()) return nil;
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

@implementation CoralWindowRestoration

+ (void)restoreWindowWithIdentifier:(NSString*)identifier state:(NSCoder*)state
    completionHandler:(void (^)(NSWindow*, NSError*))completionHandler {
  (void)state;
  std::shared_ptr<coral::ui::Window> window =
    coral::ui::restoreWindowForIdentifier(identifier.UTF8String);
  if (window == nullptr) {
    completionHandler(nil, [NSError errorWithDomain:@"CoralUI" code:1 userInfo:@{
      NSLocalizedDescriptionKey: @"no CoralUI restoration factory produced a window" }]);
    return;
  }
  coral::ui::retainRestoredWindow(window);
  completionHandler((__bridge NSWindow*)window->nativeWindowHandle(), nil);
}

@end

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
  if (platform != nullptr) {
    platform->ensureCommandMenu();
    platform->rebuildCommandMenu();
    platform->windowVisibilityChanged(true);
  }
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
