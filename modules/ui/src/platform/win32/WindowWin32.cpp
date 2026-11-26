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
#include <ui/Win32DragOut.h>
#include "DropTargetWin32.h"
#include "UiaProvider.h"
#include <ui/Clipboard.h>

#include <gpu/WindowSurface.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
#include <windowsx.h>
#include <imm.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

namespace coral::ui::detail {

namespace {

constexpr const wchar_t* kWindowClass = L"CoralUiWindow";
constexpr UINT_PTR kResizeTimerId = 1;

/** Enables per-monitor DPI before the first HWND exists, falling back to system awareness. */
void enableDpiAwareness() {
  using SetDpiAwarenessContext = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
  if (const HMODULE user32 = GetModuleHandleW(L"user32.dll"); user32 != nullptr) {
    const auto setContext = reinterpret_cast<SetDpiAwarenessContext>(
      GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (setContext != nullptr) {
      // -4 is DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2; using the numeric value keeps older SDKs
      // compiling while new Windows versions get per-monitor behaviour.
      if (setContext(reinterpret_cast<DPI_AWARENESS_CONTEXT>(-4)) != FALSE) {
        return;
      }
    }
  }
  SetProcessDPIAware();
}

/** The DPI of the monitor the HWND is on, not the process/system DPI. */
float dpiScaleForWindow(const HWND hwnd) {
  using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
  if (const HMODULE user32 = GetModuleHandleW(L"user32.dll"); user32 != nullptr && hwnd != nullptr) {
    const auto getDpi = reinterpret_cast<GetDpiForWindowFn>(
      GetProcAddress(user32, "GetDpiForWindow"));
    if (getDpi != nullptr) {
      const UINT dpi = getDpi(hwnd);
      if (dpi > 0) {
        return static_cast<float>(dpi) / 96.0f;
      }
    }
  }
  if (const HDC dc = GetDC(hwnd); dc != nullptr) {
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(hwnd, dc);
    if (dpi > 0) {
      return static_cast<float>(dpi) / 96.0f;
    }
  }
  return 1.0f;
}

Key keyFromVirtualKey(const WPARAM key, const LPARAM keyData) {
  // The extended flag and the scan code are the only way Windows tells the twin keys apart: the
  // virtual-key code for the right-hand Ctrl/Alt and the numpad Enter is the generic one.
  const bool extended = (keyData & (1L << 24)) != 0;
  const auto scancode = static_cast<UINT>((keyData >> 16) & 0xFF);
  if (key >= 'A' && key <= 'Z') {
    return static_cast<Key>(static_cast<uint16_t>(Key::A) + static_cast<uint16_t>(key - 'A'));
  }
  if (key >= '0' && key <= '9') {
    return static_cast<Key>(static_cast<uint16_t>(Key::Digit0) + static_cast<uint16_t>(key - '0'));
  }
  if (key >= VK_F1 && key <= VK_F12) {
    return static_cast<Key>(static_cast<uint16_t>(Key::F1) +
      static_cast<uint16_t>(key - VK_F1));
  }
  switch (key) {
    case VK_ESCAPE:
      return Key::Escape;
    case VK_RETURN:
      // The keypad Enter arrives as VK_RETURN without the extended flag.
      return extended ? Key::Enter : Key::NumpadEnter;
    case VK_SPACE:
      return Key::Space;
    case VK_TAB:
      return Key::Tab;
    case VK_BACK:
      return Key::Backspace;
    case VK_LEFT:
      return Key::Left;
    case VK_RIGHT:
      return Key::Right;
    case VK_UP:
      return Key::Up;
    case VK_DOWN:
      return Key::Down;
    case VK_DELETE:
      return Key::Delete;
    case VK_HOME:
      return Key::Home;
    case VK_END:
      return Key::End;
    case VK_PRIOR:
      return Key::PageUp;
    case VK_NEXT:
      return Key::PageDown;
    case VK_INSERT:
      return Key::Insert;
    case VK_OEM_COMMA:
      return Key::Comma;
    case VK_OEM_PERIOD:
      return Key::Period;
    case VK_OEM_2:
      return Key::Slash;
    case VK_OEM_5:
      return Key::Backslash;
    case VK_OEM_1:
      return Key::Semicolon;
    case VK_OEM_7:
      return Key::Apostrophe;
    case VK_OEM_4:
      return Key::LeftBracket;
    case VK_OEM_6:
      return Key::RightBracket;
    case VK_OEM_MINUS:
      return Key::Minus;
    case VK_OEM_PLUS:
      return Key::Equal;
    case VK_OEM_3:
      return Key::Grave;
    case VK_SHIFT:
      return scancode == 0x36 ? Key::RightShift : Key::LeftShift;
    case VK_LSHIFT:
      return Key::LeftShift;
    case VK_RSHIFT:
      return Key::RightShift;
    case VK_CONTROL:
      return extended ? Key::RightControl : Key::LeftControl;
    case VK_LCONTROL:
      return Key::LeftControl;
    case VK_RCONTROL:
      return Key::RightControl;
    case VK_MENU:
      return extended ? Key::RightAlt : Key::LeftAlt;
    case VK_LMENU:
      return Key::LeftAlt;
    case VK_RMENU:
      return Key::RightAlt;
    case VK_LWIN:
      return Key::LeftMeta;
    case VK_RWIN:
      return Key::RightMeta;
    case VK_CAPITAL:
      return Key::CapsLock;
    case VK_NUMLOCK:
      return Key::NumLock;
    case VK_SCROLL:
      return Key::ScrollLock;
    case VK_SNAPSHOT:
      return Key::PrintScreen;
    case VK_PAUSE:
      return Key::Pause;
    case VK_APPS:
      return Key::Menu;
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
      return static_cast<Key>(static_cast<uint16_t>(Key::Numpad0) +
        static_cast<uint16_t>(key - VK_NUMPAD0));
    case VK_MULTIPLY:
      return Key::NumpadMultiply;
    case VK_ADD:
      return Key::NumpadAdd;
    case VK_SUBTRACT:
      return Key::NumpadSubtract;
    case VK_DECIMAL:
      return Key::NumpadDecimal;
    case VK_DIVIDE:
      return Key::NumpadDivide;
    case VK_OEM_NEC_EQUAL:
      return Key::NumpadEqual;
    default:
      return Key::Unknown;
  }
}

std::string utf16ToUtf8(const std::wstring& wide) {
  const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
    nullptr, 0, nullptr, nullptr);
  if (bytes <= 0) return {};
  std::string utf8(static_cast<size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), bytes,
    nullptr, nullptr);
  return utf8;
}

std::wstring utf8ToUtf16(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int chars = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
    nullptr, 0);
  if (chars <= 0) return {};
  std::wstring wide(static_cast<size_t>(chars), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), chars);
  return wide;
}

} // namespace anonymous

[[nodiscard]] HCURSOR loadCursor(const Window::Cursor cursor) {
  // Numeric IDs: the IDC_* macros are ANSI unless the build defines UNICODE, while the window layer
  // always calls the W APIs.
  int id = 32512;  // IDC_ARROW
  switch (cursor) {
    case Window::Cursor::IBeam: id = 32513; break;
    case Window::Cursor::Wait: id = 32514; break;
    case Window::Cursor::Crosshair: id = 32515; break;
    case Window::Cursor::ResizeHorizontal: id = 32644; break;
    case Window::Cursor::ResizeVertical: id = 32645; break;
    case Window::Cursor::Move: id = 32646; break;
    case Window::Cursor::NotAllowed: id = 32648; break;
    case Window::Cursor::Hand: id = 32649; break;
    case Window::Cursor::Arrow:
    default: id = 32512; break;
  }
  return LoadCursorW(nullptr, MAKEINTRESOURCEW(id));
}

class Win32Platform final : public WindowPlatform {
public:
  Win32Platform(Window::Options options, backend::Device* sharedDevice)
    : mOptions(std::move(options)), mSharedDevice(sharedDevice), mWidth(mOptions.width),
      mHeight(mOptions.height) {
  }

  ~Win32Platform() override {
    if (mOwnsComApartment) {
      ::CoUninitialize();
    }
    if (mHwnd != nullptr) {
      KillTimer(mHwnd, kResizeTimerId);
      if (mDropTargetReady) RemoveClipboardFormatListener(mHwnd);
    }
    // Revoke the OLE target before the window disappears; the wrapper owns the OLE lifetime.
    mDropTarget.reset();
    mSurface.reset();
    if (mHwnd != nullptr) {
      SetWindowLongPtrW(mHwnd, GWLP_USERDATA, 0);
      DestroyWindow(mHwnd);
      mHwnd = nullptr;
    }
  }

  bool initialize(std::string* diagnostics) {
    enableDpiAwareness();
    // UI Automation answers WM_GETOBJECT through the COM-hosted UIA service, so the UI thread has to
    // be in an apartment before UiaReturnRawElementProvider is called; a platform that never initialised
    // COM makes the client wait for a response that can never arrive. S_FALSE means this thread was
    // already in an STA, and RPC_E_CHANGED_MODE means it is in an MTA -- in both cases the apartment is
    // not ours to uninitialise.
    const HRESULT comResult = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    mComInitialized = comResult == S_OK || comResult == S_FALSE;
    mOwnsComApartment = comResult == S_OK;
    static bool registered = false;
    if (!registered) {
      WNDCLASSEXW description{};
      description.cbSize = sizeof(description);
      description.style = CS_HREDRAW | CS_VREDRAW;
      description.lpfnWndProc = &Win32Platform::windowProc;
      description.hInstance = GetModuleHandleW(nullptr);
      description.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
      description.lpszClassName = kWindowClass;
      if (RegisterClassExW(&description) == 0) {
        if (diagnostics != nullptr) *diagnostics = "the UI window class could not be registered";
        return false;
      }
      registered = true;
    }
    RECT rectangle{ 0, 0, mOptions.width, mOptions.height };
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                        WS_THICKFRAME | WS_MAXIMIZEBOX;
    AdjustWindowRectEx(&rectangle, style, FALSE, 0);
    const std::wstring title = utf8ToUtf16(mOptions.title);
    // A borderless-HDR acceptance run has to open on the HDR display, so the position is honoured when
    // the caller names one; CW_USEDEFAULT keeps the previous behaviour for every other window.
    const int windowX = mOptions.position.has_value() ? static_cast<int>(mOptions.position->x) : CW_USEDEFAULT;
    const int windowY = mOptions.position.has_value() ? static_cast<int>(mOptions.position->y) : CW_USEDEFAULT;
    mHwnd = CreateWindowExW(0, kWindowClass, title.c_str(), style, windowX, windowY,
      rectangle.right - rectangle.left, rectangle.bottom - rectangle.top, nullptr, nullptr,
      GetModuleHandleW(nullptr), nullptr);
    if (mHwnd == nullptr) {
      if (diagnostics != nullptr) *diagnostics = "the UI window could not be created";
      return false;
    }
    SetWindowLongPtrW(mHwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    DragAcceptFiles(mHwnd, TRUE);
    ShowWindow(mHwnd, SW_SHOW);
    UpdateWindow(mHwnd);
    // The monitor DPI is authoritative; GetDeviceCaps on the window DC can still report the system
    // DPI on a per-monitor-aware process that was created on a secondary monitor.
    mDpiScale = dpiScaleForWindow(mHwnd);
    if (mOptions.renderHost != RenderHostKind::RenderThread && !createSurface()) {
      if (diagnostics != nullptr) *diagnostics = "the GPU window surface could not be created";
      return false;
    }
    return true;
  }

  [[nodiscard]] Window::NativePlatform kind() const override {
    return Window::NativePlatform::Win32;
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
    if (!mDropTargetReady) {
      // OLE drag/drop and clipboard notifications need the window host, which only exists once the
      // window is being pumped; register them on the first pump.
      mDropTargetReady = true;
      mDropTarget = detail::createWin32DropTarget(mHwnd, &host);
      AddClipboardFormatListener(mHwnd);
    }
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      if (message.message == WM_QUIT) return false;
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    return true;
  }

  void waitForEvents(const int timeoutMs) override {
    if (mHwnd == nullptr) return;
    MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(timeoutMs), QS_ALLINPUT, 0);
  }

  void wake() override {
    if (mHwnd != nullptr) {
      // A message no handler owns: it exists only to make the wait return, and DefWindowProc ignores it.
      PostMessageW(mHwnd, WM_APP + 0x4D, 0, 0);
    }
  }

  [[nodiscard]] graphics::Canvas* beginFrame() override {
    if (mSurface == nullptr) {
      return nullptr;
    }
    // WM_SIZE only records the new client size. The swapchain is reconciled once per frame, after
    // the modal sizing loop has had a chance to coalesce several resize messages into one frame.
    if (mSurface->width() != static_cast<uint32_t>(mWidth) ||
        mSurface->height() != static_cast<uint32_t>(mHeight)) {
      if (!mSurface->resize(static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight))) {
        mDiagnostics = mSurface->diagnostics();
        return nullptr;
      }
    }
    return mSurface->beginFrame();
  }

  /**
   * The window surface can present a frame built from several lists, but that path changes how the frame
   * reaches the swapchain, so it stays behind a flag until a windowed run verifies it.
   */
  [[nodiscard]] bool supportsBoundarySubmission() const override {
    static const bool enabled = std::getenv("CORAL_UI_FRAME_PLAN_WINDOW") != nullptr;
    return enabled && mSurface != nullptr;
  }
  bool presentBoundaries(const std::vector<graphics::gpu::Surface::ListItem>& items) override {
    if (mSurface == nullptr) return false;
    // ## The window path honours each item's translation and clip
    //
    // This used to take `item.list` and drop the rest, which drew a scrolled viewport's band wherever
    // the content happened to be and without its viewport clip -- the reason the plan path could not be
    // enabled on Windows (`CORAL_UI_FRAME_PLAN_WINDOW`). The window surface now bakes a whole-pixel
    // translation and the clip into the list and submits every list of the frame in one command buffer.
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
  [[nodiscard]] graphics::gpu::FrameStages lastFrameStages() const override {
    return mSurface != nullptr ? mSurface->lastFrameStages() : graphics::gpu::FrameStages{};
  }
  [[nodiscard]] uint32_t lastFrameDrawCalls() const override {
    return mSurface != nullptr ? mSurface->lastFrameDrawCallCount() : 0;
  }
  [[nodiscard]] uint32_t lastFrameBatches() const override {
    return mSurface != nullptr ? mSurface->lastFrameBatchCount() : 0;
  }

  bool present() override {
    if (mSurface == nullptr) return false;
    const bool ok = mSurface->present();
    if (!ok) mDiagnostics = mSurface->diagnostics();
    return ok;
  }
  [[nodiscard]] bool supportsRetainedFrames() const override { return true; }
  bool presentRetained() override {
    return mSurface != nullptr && mSurface->presentRetained();
  }
  DragEffect beginDragOut(const DragPayload& payload, const DragEffect allowed) override {
    return runWin32Drag(payload, allowed);
  }
  [[nodiscard]] bool supportsDamageRects() const override { return true; }
  bool presentDamage(const Rect& damage) override {
    return mSurface != nullptr && mSurface->presentDamage(damage);
  }

  [[nodiscard]] bool snapshot(graphics::Bitmap* out) override {
    return mSurface != nullptr && mSurface->readback(out);
  }

  bool resize(uint32_t width, uint32_t height) override {
    mWidth = static_cast<int>(width);
    mHeight = static_cast<int>(height);
    if (mOptions.renderHost != RenderHostKind::RenderThread && mSurface == nullptr) {
      mDiagnostics = "no window surface";
      return false;
    }
    if (mHwnd != nullptr) {
      // `SetWindowPos` takes the *outer* rectangle, and on a DPI-scaled window it is expressed in
      // logical units. Convert once, then correct from the real client rectangle: the surface and
      // swapchain must match the client size, or the next present returns OutOfDate.
      const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(mHwnd, GWL_STYLE));
      const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(mHwnd, GWL_EXSTYLE));
      RECT rectangle{ 0, 0, mWidth, mHeight };
      AdjustWindowRectEx(&rectangle, style, FALSE, exStyle);
      SetWindowPos(mHwnd, nullptr, 0, 0, rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

      RECT client{};
      GetClientRect(mHwnd, &client);
      const int targetWidth = mWidth;
      const int targetHeight = mHeight;
      // DPI virtualization can make one correction land a few pixels off. Iterate against the real
      // client rectangle so `resize` means device pixels; if the window manager clamps (a minimum
      // track size), the actual size is what `width()`/`height()` report.
      for (int attempt = 0; attempt < 4; ++attempt) {
        const int clientWidth = client.right - client.left;
        const int clientHeight = client.bottom - client.top;
        if (clientWidth == targetWidth && clientHeight == targetHeight) break;
        RECT outer{};
        GetWindowRect(mHwnd, &outer);
        SetWindowPos(mHwnd, nullptr, 0, 0,
          (outer.right - outer.left) + (targetWidth - clientWidth),
          (outer.bottom - outer.top) + (targetHeight - clientHeight),
          SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        GetClientRect(mHwnd, &client);
      }
      mWidth = client.right - client.left;
      mHeight = client.bottom - client.top;
    }
    if (mOptions.renderHost == RenderHostKind::RenderThread) {
      // The render thread owns the swapchain; `Window::resize` forwards the final client size to
      // the host, which resizes the surface there.
      return true;
    }
    if (!mSurface->resize(static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight))) {
      mDiagnostics = mSurface->diagnostics();
      return false;
    }
    return true;
  }

  bool recreate() override {
    mSurface.reset();
    return createSurface();
  }

  void setTitle(std::string_view title) override {
    if (mHwnd == nullptr) return;
    const std::wstring wide = utf8ToUtf16(std::string(title));
    SetWindowTextW(mHwnd, wide.c_str());
  }

  void setPosition(const int x, const int y) override {
    if (mHwnd == nullptr) return;
    SetWindowPos(mHwnd, nullptr, x, y, 0, 0,
      SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
  }

  void setMinimumSize(const int width, const int height) override {
    mMinWidth = std::max(0, width);
    mMinHeight = std::max(0, height);
  }

  // ================================================================================================
  // NativeViewLayer host
  // ================================================================================================
  // A native control is a real child HWND. The layer tree only supplies its logical rectangle and
  // visibility: the child paints itself through USER32 and always sits above the GPU content, the
  // same airspace model as an NSView above the Cocoa/Metal view. The `ui/win32` wrapper owns the
  // HWND; this backend only positions, shows, hides and detaches it.
  void setNativeViewBounds(void* nativeView, const Rect bounds, const bool visible) override {
    HWND child = static_cast<HWND>(nativeView);
    if (child == nullptr || !::IsWindow(child) || mHwnd == nullptr) return;

    if (::GetParent(child) != mHwnd) {
      const LONG_PTR style = ::GetWindowLongPtrW(child, GWL_STYLE);
      if ((style & WS_CHILD) == 0) {
        ::SetWindowLongPtrW(child, GWL_STYLE, (style | WS_CHILD) & ~WS_POPUP);
      }
      ::SetParent(child, mHwnd);
      mNativeViews.erase(child);
    }

    const int x = static_cast<int>(std::lround(bounds.getLeft() * mDpiScale));
    const int y = static_cast<int>(std::lround(bounds.getTop() * mDpiScale));
    const int width = std::max(1, static_cast<int>(std::lround(bounds.getWidth() * mDpiScale)));
    const int height = std::max(1, static_cast<int>(std::lround(bounds.getHeight() * mDpiScale)));
    const RECT target{ x, y, x + width, y + height };

    NativeViewState& state = mNativeViews[child];
    if (state.epoch == mNativeViewEpoch && state.visible == visible &&
        state.bounds.left == target.left && state.bounds.top == target.top &&
        state.bounds.right == target.right && state.bounds.bottom == target.bottom) {
      return;
    }
    state.bounds = target;
    state.visible = visible;
    state.epoch = mNativeViewEpoch;
    ::SetWindowPos(child, HWND_TOP, x, y, width, height,
      SWP_NOACTIVATE | SWP_NOOWNERZORDER |
        (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
  }

  // The layer borrows the HWND; its owner (the `ui/win32` wrapper) destroys it. Win32 only hides and
  // detaches the child here so a wrapper that outlives the window can still tear it down, and a
  // headless window (whose platform has no native-view hook) does not leak a desktop child.
  void removeNativeView(void* nativeView) override {
    HWND child = static_cast<HWND>(nativeView);
    if (child == nullptr) return;
    mNativeViews.erase(child);
    if (::IsWindow(child)) {
      ::ShowWindow(child, SW_HIDE);
      ::SetParent(child, ::GetDesktopWindow());
    }
  }

  void setMaximized(bool maximized) override {
    if (mHwnd != nullptr) ShowWindow(mHwnd, maximized ? SW_MAXIMIZE : SW_RESTORE);
  }

  void setMinimized(bool minimized) override {
    if (mHwnd != nullptr) ShowWindow(mHwnd, minimized ? SW_MINIMIZE : SW_RESTORE);
  }

  void setDecorations(Window::Decorations decorations) override {
    mClientDecorations = decorations == Window::Decorations::Client;
    if (mHwnd == nullptr) return;
    // Recompute the non-client area: WM_NCCALCSIZE then makes the client cover the whole window.
    SetWindowPos(mHwnd, nullptr, 0, 0, 0, 0,
      SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  }

  void setCursor(Window::Cursor cursor) override {
    mCursor = loadCursor(cursor);
    if (mHwnd != nullptr && mCursor != nullptr) ::SetCursor(mCursor);
  }

  void setOpacity(float opacity) override {
    // Layered windows and a hardware swapchain do not mix reliably; keep the value (Window owns it)
    // and let a compositor-era backend apply it later (P3-7).
    (void)opacity;
  }

  [[nodiscard]] bool prefersDarkAppearance() const override {
    DWORD value = 1;
    DWORD size = sizeof(value);
    DWORD type = 0;
    if (RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, &type, &value, &size) != ERROR_SUCCESS) {
      return false;
    }
    return value == 0;
  }

  [[nodiscard]] SystemAppearance systemAppearance() const override {
    SystemAppearance appearance;
    appearance.valid = true;
    appearance.preferDark = prefersDarkAppearance();
    // High contrast is a system-wide switch with its own palette; `SPI_GETHIGHCONTRAST` is what the
    // shell's own controls read, and `GetSysColor` returns the palette that goes with it.
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) != FALSE) {
      appearance.highContrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    }
    // "Animation effects" off means animations should snap, not play. The window applies this
    // process-wide, because an animation starts from a layer that does not know its window.
    BOOL animations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0) != FALSE) {
      appearance.reduceMotion = animations == FALSE;
    }
    if (appearance.highContrast) {
      const auto toArgb = [](const COLORREF color) noexcept {
        return 0xFF000000u | (static_cast<uint32_t>(GetRValue(color)) << 16) |
          (static_cast<uint32_t>(GetGValue(color)) << 8) | static_cast<uint32_t>(GetBValue(color));
      };
      SystemColors& colors = appearance.colors;
      colors.valid = true;
      colors.window = toArgb(GetSysColor(COLOR_WINDOW));
      colors.windowText = toArgb(GetSysColor(COLOR_WINDOWTEXT));
      colors.buttonFace = toArgb(GetSysColor(COLOR_BTNFACE));
      colors.buttonText = toArgb(GetSysColor(COLOR_BTNTEXT));
      colors.highlight = toArgb(GetSysColor(COLOR_HIGHLIGHT));
      colors.highlightText = toArgb(GetSysColor(COLOR_HIGHLIGHTTEXT));
      colors.grayText = toArgb(GetSysColor(COLOR_GRAYTEXT));
    }
    return appearance;
  }

  void setAlwaysOnTop(bool onTop) override {
    if (mHwnd == nullptr) return;
    SetWindowPos(mHwnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  void setFullscreen(const bool fullscreen) override {
    if (mHwnd == nullptr) return;
    if (fullscreen && !mFullscreen) {
      GetWindowRect(mHwnd, &mRestoreRect);
      mRestoreStyle = GetWindowLongPtrW(mHwnd, GWL_STYLE);
      MONITORINFO monitor{};
      monitor.cbSize = sizeof(monitor);
      GetMonitorInfoW(MonitorFromWindow(mHwnd, MONITOR_DEFAULTTONEAREST), &monitor);
      SetWindowLongPtrW(mHwnd, GWL_STYLE, (mRestoreStyle & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
      SetWindowPos(mHwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
        monitor.rcMonitor.right - monitor.rcMonitor.left,
        monitor.rcMonitor.bottom - monitor.rcMonitor.top,
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
      mFullscreen = true;
    } else if (!fullscreen && mFullscreen) {
      SetWindowLongPtrW(mHwnd, GWL_STYLE, mRestoreStyle);
      SetWindowPos(mHwnd, nullptr, mRestoreRect.left, mRestoreRect.top,
        mRestoreRect.right - mRestoreRect.left, mRestoreRect.bottom - mRestoreRect.top,
        SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
      mFullscreen = false;
    }
  }

  [[nodiscard]] Rect screenBounds() const override {
    if (mHwnd == nullptr) return Rect{};
    RECT rectangle{};
    GetWindowRect(mHwnd, &rectangle);
    return Rect::MakeLTRB(static_cast<float>(rectangle.left), static_cast<float>(rectangle.top),
      static_cast<float>(rectangle.right), static_cast<float>(rectangle.bottom));
  }

  void raise() override {
    if (mHwnd == nullptr) return;
    SetWindowPos(mHwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER);
    SetForegroundWindow(mHwnd);
    SetActiveWindow(mHwnd);
  }

  [[nodiscard]] std::string diagnostics() const override {
    return mDiagnostics;
  }

  [[nodiscard]] graphics::gpu::NativeDisplay nativeDisplay() const override {
    graphics::gpu::NativeDisplay native;
    native.kind = graphics::gpu::NativeDisplayKind::Windows;
    native.display = mHwnd;
    return native;
  }

  [[nodiscard]] void* nativeWindow() const override { return mHwnd; }

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
    if (mHwnd == nullptr || rect.getWidth() <= 0.0f || rect.getHeight() <= 0.0f) {
      return;
    }
    const HIMC context = ImmGetContext(mHwnd);
    if (context == nullptr) {
      return;
    }
    const LONG x = static_cast<LONG>(std::lround(rect.getLeft() * mDpiScale));
    const LONG y = static_cast<LONG>(std::lround(rect.getTop() * mDpiScale));
    COMPOSITIONFORM composition{};
    composition.dwStyle = CFS_POINT;
    composition.ptCurrentPos = { x, y };
    ImmSetCompositionWindow(context, &composition);
    CANDIDATEFORM candidate{};
    candidate.dwStyle = CFS_CANDIDATEPOS;
    candidate.ptCurrentPos = { x, y };
    ImmSetCandidateWindow(context, &candidate);
    ImmReleaseContext(mHwnd, context);
  }

private:
  bool createSurface() {
    graphics::gpu::WindowSurface::Configuration configuration;
  configuration.hdrOutput = mOptions.hdrOutput;
    configuration.nativeWindow = mHwnd;
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

  void setAccessibilityTreeProvider(std::function<AccessibilityNode()> provider,
      AccessibilityActionInvoker invoker) override {
    mAccessibilityProvider = std::move(provider);
    mAccessibilityInvoker = std::move(invoker);
  }

  void raiseAccessibilityFocusEvent() override {
    if (!mAccessibilityProvider || mHwnd == nullptr) return;
    const bool uiaLog = std::getenv("CORAL_UI_UIA_LOG") != nullptr;
    AccessibilityNode snapshot = mAccessibilityProvider();
    std::string path;
    const auto findFocused = [&path](const auto& self, const AccessibilityNode& node) -> void {
      if (node.focused) { path = node.path; return; }
      for (const AccessibilityNode& child : node.children) self(self, child);
    };
    findFocused(findFocused, snapshot);
    if (path.empty()) return;
    auto* element = static_cast<IRawElementProviderSimple*>(
      createUiaElementProvider(mHwnd, std::move(snapshot), path));
    if (element != nullptr) {
      // UIA_AutomationFocusChangedEventId, defined in the client header rather than the provider one.\n      UiaRaiseAutomationEvent(element, 20005);
      element->Release();
    }
    ::NotifyWinEvent(EVENT_OBJECT_FOCUS, mHwnd, OBJID_CLIENT, 0);
    if (uiaLog) std::fprintf(stderr, "[uia] focus event path=%s\n", path.c_str());
  }

  void raiseAccessibilityPropertyEvent(const std::string_view path, const AccessibilityEvent event) override {
    if (!mAccessibilityProvider || mHwnd == nullptr) return;
    if (event == AccessibilityEvent::ChildrenAdded || event == AccessibilityEvent::ChildrenRemoved) {
      // A structure change is raised on the parent element: the new/removed child does not exist in the
      // previous snapshot, so it cannot be addressed as an element yet.
      AccessibilityNode snapshot = mAccessibilityProvider();
      auto* element = static_cast<IRawElementProviderSimple*>(
        createUiaElementProvider(mHwnd, std::move(snapshot), std::string(path)));
      if (element == nullptr) return;
      const StructureChangeType change = event == AccessibilityEvent::ChildrenAdded
        ? StructureChangeType_ChildAdded : StructureChangeType_ChildRemoved;
      UiaRaiseStructureChangedEvent(element, change, nullptr, 0);
      element->Release();
      if (std::getenv("CORAL_UI_UIA_LOG")) {
        std::fprintf(stderr, "[uia] structure event path=%s added=%d\n", std::string(path).c_str(),
          event == AccessibilityEvent::ChildrenAdded ? 1 : 0);
      }
      return;
    }
    AccessibilityNode snapshot = mAccessibilityProvider();
    const AccessibilityNode* node = nullptr;
    const auto findNode = [&node, &path](const auto& self, const AccessibilityNode& current) -> void {
      if (current.path == path) { node = &current; return; }
      for (const AccessibilityNode& child : current.children) {
        if (node == nullptr) self(self, child);
      }
    };
    findNode(findNode, snapshot);
    if (node == nullptr) return;
    auto* element = static_cast<IRawElementProviderSimple*>(
      createUiaElementProvider(mHwnd, snapshot, std::string(path)));
    if (element == nullptr) return;

    VARIANT oldValue;
    VARIANT newValue;
    VariantInit(&oldValue);
    VariantInit(&newValue);
    PROPERTYID propertyId = 30045; // UIA_ValueValuePropertyId
    if (event == AccessibilityEvent::ToggleChanged) {
      propertyId = 30086; // UIA_ToggleToggleStatePropertyId
      newValue.vt = VT_I4;
      newValue.lVal = node->selected ? 1 : 0;
    } else if (event == AccessibilityEvent::Selected) {
      propertyId = 30079; // UIA_SelectionItemIsSelectedPropertyId
      newValue.vt = VT_BOOL;
      newValue.boolVal = node->selected ? VARIANT_TRUE : VARIANT_FALSE;
    } else if (event == AccessibilityEvent::EnabledChanged) {
      propertyId = 30010; // UIA_IsEnabledPropertyId
      newValue.vt = VT_BOOL;
      newValue.boolVal = node->enabled ? VARIANT_TRUE : VARIANT_FALSE;
    } else {
      const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, node->value.c_str(),
        static_cast<int>(node->value.size()), nullptr, 0);
      std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
      if (wideLength > 0) {
        ::MultiByteToWideChar(CP_UTF8, 0, node->value.c_str(),
          static_cast<int>(node->value.size()), wide.data(), wideLength);
      }
      newValue.vt = VT_BSTR;
      newValue.bstrVal = ::SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
    }
    UiaRaiseAutomationPropertyChangedEvent(element, propertyId, oldValue, newValue);
    VariantClear(&newValue);
    element->Release();
    if (std::getenv("CORAL_UI_UIA_LOG")) {
      std::fprintf(stderr, "[uia] property event path=%s property=%ld\n", std::string(path).c_str(),
        static_cast<long>(propertyId));
    }
  }

  static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* platform = reinterpret_cast<Win32Platform*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (platform == nullptr || platform->mHost == nullptr) {
      return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    WindowHost& host = *platform->mHost;
    if (message == WM_DROPFILES) {
      const auto drop = reinterpret_cast<HDROP>(wParam);
      const UINT count = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
      POINT point{};
      DragQueryPoint(drop, &point);
      DropEvent event;
      event.position = { static_cast<float>(point.x), static_cast<float>(point.y) };
      for (UINT index = 0; index < count; ++index) {
        const UINT length = DragQueryFileW(drop, index, nullptr, 0);
        if (length == 0) continue;
        std::wstring path(static_cast<size_t>(length) + 1, L'\0');
        DragQueryFileW(drop, index, path.data(), length + 1);
        path.resize(length);
        event.paths.push_back(utf16ToUtf8(path));
      }
      DragFinish(drop);
      host.dispatchDrop(event);
      return 0;
    }
    switch (message) {
      case WM_GETOBJECT: {
        // UI Automation asks for its root object with UiaRootObjectId; the snapshot is taken here, on
        // the UI thread, and the provider keeps an immutable copy for the calls that follow.
        // On by default: the client handshake is verified (AutomationElement.FromHandle reads the tree
        // and its names/bounds). CORAL_UI_UIA=0 opts out for a product that does not want to expose a
        // provider, e.g. one that ships its own accessibility bridge.
        const char* uiaSetting = std::getenv("CORAL_UI_UIA");
        const bool uiaEnabled = uiaSetting == nullptr || std::string(uiaSetting) != "0";
        const bool uiaLog = std::getenv("CORAL_UI_UIA_LOG") != nullptr;
        if (uiaLog) std::fprintf(stderr, "[uia] WM_GETOBJECT lParam=%ld provider=%d\n",
          static_cast<long>(lParam), platform->mAccessibilityProvider ? 1 : 0);
        if (uiaEnabled && static_cast<LONG>(lParam) == UiaRootObjectId && platform->mAccessibilityProvider) {
          auto* provider = static_cast<IRawElementProviderSimple*>(
            createUiaRootProvider(hwnd, platform->mAccessibilityProvider(),
              platform->mAccessibilityInvoker));
          if (provider != nullptr) {
            const LRESULT result = UiaReturnRawElementProvider(hwnd, wParam, lParam, provider);
            provider->Release();
            if (uiaLog) std::fprintf(stderr, "[uia] provider=%p result=%lld\n", static_cast<void*>(provider),
              static_cast<long long>(result));
            return result;
          }
        }
        break;
      }
      case WM_GETMINMAXINFO: {
        if (platform->mMinWidth > 0 || platform->mMinHeight > 0) {
          auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
          RECT minimum{ 0, 0, std::max(1, platform->mMinWidth), std::max(1, platform->mMinHeight) };
          AdjustWindowRectEx(&minimum, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE, 0);
          if (platform->mMinWidth > 0) info->ptMinTrackSize.x = minimum.right - minimum.left;
          if (platform->mMinHeight > 0) info->ptMinTrackSize.y = minimum.bottom - minimum.top;
          return 0;
        }
        break;
      }
      case WM_CLOSE:
        host.requestClose();
        return 0;
      case WM_DESTROY:
        host.requestClose();
        PostQuitMessage(0);
        return 0;
      case WM_ERASEBKGND:
        return 1;
      case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd, &paint);
        EndPaint(hwnd, &paint);
        return 0;
      }
      case WM_SYSCOLORCHANGE:
    case WM_THEMECHANGED:
    case WM_SETTINGCHANGE: {
      // High contrast, the app theme and the animation policy are all system settings: report the
      // event and let the window re-read `systemAppearance()` and apply it. Re-reading is cheap and
      // idempotent, so the many unrelated `WM_SETTINGCHANGE` broadcasts need no filtering by name.
      host.systemAppearanceChanged();
      host.requestFrame();
      return 0;
    }
    case WM_DPICHANGED: {
        const UINT dpi = HIWORD(wParam);
        platform->mDpiScale = dpi > 0 ? static_cast<float>(dpi) / 96.0f : 1.0f;
        // Native child frames are cached in device pixels; force the next sync to reposition them at
        // the new scale even when the logical rectangle did not change.
        ++platform->mNativeViewEpoch;
        platform->mNativeViews.clear();
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested != nullptr) {
          SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
            suggested->right - suggested->left, suggested->bottom - suggested->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        }
        // A monitor move can change the DPI without changing the client size, so WM_SIZE is not a
        // reliable place to refresh the UI scale. Re-run layout and present once at the new scale.
        if (platform->mWidth > 0 && platform->mHeight > 0) {
          host.platformResized(platform->mWidth, platform->mHeight);
          host.renderFrameNow();
        }
        return 0;
      }
      case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
          const int width = static_cast<int>(LOWORD(lParam));
          const int height = static_cast<int>(HIWORD(lParam));
          if (width > 0 && height > 0) {
            platform->mWidth = width;
            platform->mHeight = height;
            // Do not rebuild the swapchain from inside the modal sizing loop. beginFrame applies
            // the coalesced size and the resize timer below keeps presenting while the loop runs.
            host.platformResized(width, height);
          }
        }
        return 0;
      case WM_ENTERSIZEMOVE:
        SetTimer(hwnd, kResizeTimerId, 16, nullptr);
        return 0;
      case WM_TIMER:
        if (wParam == kResizeTimerId) {
          host.renderFrameNow();
          return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
      case WM_EXITSIZEMOVE:
        KillTimer(hwnd, kResizeTimerId);
        return 0;
      case WM_POINTERDOWN:
      case WM_POINTERUPDATE:
      case WM_POINTERUP: {
        // ## Touch and pen arrive as pointer messages, mouse stays on WM_MOUSE
        //
        // With `EnableMouseInPointer` off (the default) the system sends WM_POINTER for touch and pen
        // and WM_MOUSE for a mouse, so the two paths do not double-deliver. Handling WM_POINTER here
        // also stops the system from promoting a touch contact into a synthetic mouse click.
        const UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
        POINTER_INPUT_TYPE pointerType = PT_POINTER;
        if (::GetPointerType(pointerId, &pointerType) == FALSE) return 0;
        POINTER_INFO info{};
        if (::GetPointerInfo(pointerId, &info) == FALSE) return 0;
        if ((info.pointerFlags & POINTER_FLAG_INRANGE) == 0) return 0;
        POINT point = info.ptPixelLocation;
        ScreenToClient(hwnd, &point);
        PointerEvent event;
        event.position = { static_cast<float>(point.x), static_cast<float>(point.y) };
        event.kind = pointerType == PT_TOUCH ? PointerKind::Touch
          : (pointerType == PT_PEN ? PointerKind::Pen : PointerKind::Mouse);
        event.pointerId = pointerId;
        // Pressure lives on the pen/touch structs, not on the shared pointer info; a mouse has none.
        // A touch contact that reports no pressure is a full contact, while an idle pen reports 0 and
        // that is a real hover.
        event.pressure = 1.0f;
        if (pointerType == PT_PEN) {
          POINTER_PEN_INFO pen{};
          event.pressure = ::GetPointerPenInfo(pointerId, &pen) != FALSE
            ? std::min(1.0f, static_cast<float>(pen.pressure) / 1024.0f)
            : 0.0f;
        } else if (pointerType == PT_TOUCH) {
          POINTER_TOUCH_INFO touch{};
          event.pressure = ::GetPointerTouchInfo(pointerId, &touch) != FALSE && touch.pressure > 0
            ? std::min(1.0f, static_cast<float>(touch.pressure) / 1024.0f)
            : 1.0f;
        }
        if (message == WM_POINTERDOWN) event.button = PointerButton::Primary;
        host.dispatchPointer(event, message == WM_POINTERDOWN ? 1 : (message == WM_POINTERUP ? 2 : 0));
        return 0;
      }
      case WM_MOUSEMOVE: {
        if (!platform->mTrackingLeave) {
          // Ask Win32 for a WM_MOUSELEAVE: without it a hovered control keeps its hover state after
          // the pointer leaves the window.
          TRACKMOUSEEVENT tracking{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
          if (TrackMouseEvent(&tracking) != FALSE) platform->mTrackingLeave = true;
        }
        PointerEvent event;
        event.position = { static_cast<float>(GET_X_LPARAM(lParam)),
                           static_cast<float>(GET_Y_LPARAM(lParam)) };
        platform->mLastPointer = event.position;
        host.dispatchPointer(event, 0);
        return 0;
      }
      case WM_MOUSELEAVE: {
        platform->mTrackingLeave = false;
        PointerEvent event;
        event.position = platform->mLastPointer;
        event.inside = false;
        host.dispatchPointer(event, 0);
        return 0;
      }
      case WM_MOUSEWHEEL:
      case WM_MOUSEHWHEEL: {
        POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &point);
        ScrollEvent event;
        event.position = { static_cast<float>(point.x), static_cast<float>(point.y) };
        const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA * 48.0f;
        if (message == WM_MOUSEHWHEEL) {
          event.deltaX = delta;
        } else {
          event.deltaY = delta;
        }
        event.timestamp = std::chrono::steady_clock::time_point(
          std::chrono::milliseconds(static_cast<uint64_t>(GetMessageTime())));
        host.dispatchScroll(event);
        return 0;
      }
      case WM_CLIPBOARDUPDATE:
        Clipboard::notifySystemChanged();
        return 0;
      case WM_NCCALCSIZE:
        if (platform->mClientDecorations && wParam != FALSE) return 0;
        break;
      case WM_NCHITTEST: {
        if (!platform->mClientDecorations) break;
        POINT client{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(platform->mHwnd, &client);
        const WindowHit hit = platform->mHost != nullptr
          ? platform->mHost->dispatchHitTest(
              Point{ static_cast<float>(client.x), static_cast<float>(client.y) })
          : WindowHit::Client;
        switch (hit) {
          case WindowHit::None: return HTNOWHERE;
          case WindowHit::Caption: return HTCAPTION;
          case WindowHit::Minimize: return HTMINBUTTON;
          case WindowHit::Maximize: return HTMAXBUTTON;
          case WindowHit::Close: return HTCLOSE;
          case WindowHit::ResizeLeft: return HTLEFT;
          case WindowHit::ResizeRight: return HTRIGHT;
          case WindowHit::ResizeTop: return HTTOP;
          case WindowHit::ResizeBottom: return HTBOTTOM;
          case WindowHit::ResizeTopLeft: return HTTOPLEFT;
          case WindowHit::ResizeTopRight: return HTTOPRIGHT;
          case WindowHit::ResizeBottomLeft: return HTBOTTOMLEFT;
          case WindowHit::ResizeBottomRight: return HTBOTTOMRIGHT;
          case WindowHit::Client:
          default: return HTCLIENT;
        }
      }
      case WM_NCLBUTTONDOWN:
        if (platform->mClientDecorations) {
          if (wParam == HTCLOSE) {
            PostMessageW(platform->mHwnd, WM_CLOSE, 0, 0);
            return 0;
          }
          if (wParam == HTMINBUTTON) {
            ShowWindow(platform->mHwnd, SW_MINIMIZE);
            return 0;
          }
          if (wParam == HTMAXBUTTON) {
            ShowWindow(platform->mHwnd, IsZoomed(platform->mHwnd) ? SW_RESTORE : SW_MAXIMIZE);
            return 0;
          }
        }
        break;
      case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT && platform->mCursor != nullptr) {
          ::SetCursor(platform->mCursor);
          return TRUE;
        }
        break;
      case WM_CHAR: {
        // WM_CHAR carries one UTF-16 code unit. A non-BMP character arrives as a surrogate pair,
        // so buffer the high half and combine it; an unpaired surrogate is dropped instead of being
        // encoded as an illegal UTF-8 sequence.
        const auto unit = static_cast<uint32_t>(wParam);
        if (unit >= 0xD800u && unit <= 0xDBFFu) {
          platform->mHighSurrogate = static_cast<wchar_t>(unit);
          return 0;
        }
        if (unit >= 0xDC00u && unit <= 0xDFFFu) {
          if (platform->mHighSurrogate != 0) {
            const auto high = static_cast<char32_t>(platform->mHighSurrogate) - 0xD800u;
            const auto low = static_cast<char32_t>(unit) - 0xDC00u;
            platform->mHighSurrogate = 0;
            host.dispatchText(0x10000u + ((high << 10) | low));
          }
          return 0;
        }
        platform->mHighSurrogate = 0;
        if (unit >= 32u && unit != 127u) host.dispatchText(static_cast<char32_t>(unit));
        return 0;
      }
      case WM_UNICHAR:
        if (wParam == UNICODE_NOCHAR) return TRUE;
        if (wParam >= 32u && wParam != 127u) host.dispatchText(static_cast<char32_t>(wParam));
        return 0;
      case WM_IME_STARTCOMPOSITION:
      case WM_IME_ENDCOMPOSITION:
        host.dispatchComposition(std::string());
        return 0;
      case WM_IME_COMPOSITION: {
        const HIMC context = ImmGetContext(hwnd);
        if (context == nullptr) return 0;
        const auto read = [&](const DWORD flag) -> std::string {
          const LONG bytes = ImmGetCompositionStringW(context, flag, nullptr, 0);
          if (bytes <= 0) return {};
          std::wstring wide(static_cast<size_t>(bytes) / sizeof(wchar_t), L'\0');
          ImmGetCompositionStringW(context, flag, wide.data(), bytes);
          return utf16ToUtf8(wide);
        };
        if ((lParam & GCS_RESULTSTR) != 0) host.dispatchCommit(read(GCS_RESULTSTR));
        if ((lParam & GCS_COMPSTR) != 0) host.dispatchComposition(read(GCS_COMPSTR));
        ImmReleaseContext(hwnd, context);
        return 0;
      }
      case WM_LBUTTONDOWN:
      case WM_MBUTTONDOWN:
      case WM_RBUTTONDOWN: {
        PointerEvent event;
        event.position = { static_cast<float>(GET_X_LPARAM(lParam)),
                           static_cast<float>(GET_Y_LPARAM(lParam)) };
        event.button = message == WM_LBUTTONDOWN
                         ? PointerButton::Primary
                         : (message == WM_MBUTTONDOWN ? PointerButton::Middle
                                                      : PointerButton::Secondary);
        event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        SetCapture(hwnd);
        host.dispatchPointer(event, 1);
        return 0;
      }
      case WM_LBUTTONUP:
      case WM_MBUTTONUP:
      case WM_RBUTTONUP: {
        PointerEvent event;
        event.position = { static_cast<float>(GET_X_LPARAM(lParam)),
                           static_cast<float>(GET_Y_LPARAM(lParam)) };
        event.button = message == WM_LBUTTONUP
                         ? PointerButton::Primary
                         : (message == WM_MBUTTONUP ? PointerButton::Middle
                                                    : PointerButton::Secondary);
        host.dispatchPointer(event, 2);
        ReleaseCapture();
        return 0;
      }
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
      case WM_KEYUP:
      case WM_SYSKEYUP: {
        KeyEvent event;
        event.key = keyFromVirtualKey(wParam, lParam);
        event.repeat = (lParam & (1 << 30)) != 0;
        event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        event.control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        host.dispatchKey(event, message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
        return 0;
      }
      default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    // A case that only breaks (WM_GETOBJECT without a provider, WM_GETMINMAXINFO without minimums)
    // still has to answer the message; falling off the end of a window procedure is undefined.
    return DefWindowProcW(hwnd, message, wParam, lParam);
  }

  Window::Options mOptions;
  backend::Device* mSharedDevice = nullptr;
  HWND mHwnd = nullptr;
  bool mComInitialized = false;
  bool mOwnsComApartment = false;
  /** Built by Window so the bridge never walks the layer tree itself; called on the UI thread. */
  std::function<AccessibilityNode()> mAccessibilityProvider;
  AccessibilityActionInvoker mAccessibilityInvoker;
  std::unique_ptr<graphics::gpu::WindowSurface> mSurface;
  WindowHost* mHost = nullptr;
  HCURSOR mCursor = nullptr;
  bool mClientDecorations = false;
  std::shared_ptr<detail::Win32DropTarget> mDropTarget;
  bool mDropTargetReady = false;
  int mWidth = 0;
  int mHeight = 0;
  bool mTrackingLeave = false;
  Point mLastPointer{};
  wchar_t mHighSurrogate = 0;
  int mMinWidth = 0;
  int mMinHeight = 0;
  bool mFullscreen = false;
  LONG_PTR mRestoreStyle = 0;
  RECT mRestoreRect{};
  float mDpiScale = 1.0f;
  struct NativeViewState {
    RECT bounds{};
    bool visible = false;
    UINT epoch = 0;
  };
  std::unordered_map<HWND, NativeViewState> mNativeViews;
  UINT mNativeViewEpoch = 1;
  std::string mDiagnostics;
};

std::unique_ptr<WindowPlatform> createWin32Platform(const Window::Options& options,
  backend::Device* sharedDevice, std::string* diagnostics) {
  auto platform = std::make_unique<Win32Platform>(options, sharedDevice);
  if (!platform->initialize(diagnostics)) return nullptr;
  return platform;
}

} // namespace coral::ui::detail




