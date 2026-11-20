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

#include "ClipboardLinuxBackends.h"
#include "WakePipe.h"
#include "LinuxAppearance.h"
#include "Utf8Cursor.h"

#include <gpu/WindowSurface.h>

#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "xdg-shell-client-protocol.h"
#if defined(CORAL_UI_WAYLAND_FRACTIONAL_SCALE)
#include "fractional-scale-v1-client-protocol.h"
#endif
#if defined(CORAL_UI_WAYLAND_VIEWPORTER)
#include "viewporter-client-protocol.h"
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
#include "text-input-unstable-v1-client-protocol.h"
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
#include "text-input-unstable-v3-client-protocol.h"
#endif
#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
#include "cursor-shape-v1-client-protocol.h"
#endif
#if defined(CORAL_UI_WAYLAND_XDG_DECORATION)
#include "xdg-decoration-unstable-v1-client-protocol.h"
#endif
#if defined(CORAL_UI_WAYLAND_XDG_FOREIGN)
#include "xdg-foreign-unstable-v2-client-protocol.h"
#endif
#if defined(CORAL_UI_WAYLAND_ALPHA_MODIFIER)
#include "alpha-modifier-v1-client-protocol.h"
#endif

namespace coral::ui::detail {

namespace {

/**
 * One Wayland connection per process, shared by every window.
 *
 * EGL contexts are only shareable within one `EGLDisplay`, and a Wayland `EGLDisplay` is bound to
 * its `wl_display`. Opening a second connection for the second window gave it a different EGLDisplay,
 * so `Application`'s shared device could not create that window's surface (and the failure then fell
 * back to X11 with a Wayland device). The compositor's objects are cheap to bind again per window;
 * the connection is the part that has to be shared.
 */
std::mutex gDisplayMutex;
wl_display* gSharedDisplay = nullptr;
int gDisplayReferences = 0;

wl_display* acquireWaylandDisplay() {
  std::lock_guard<std::mutex> lock(gDisplayMutex);
  if (gSharedDisplay == nullptr) gSharedDisplay = wl_display_connect(nullptr);
  if (gSharedDisplay != nullptr) ++gDisplayReferences;
  return gSharedDisplay;
}

void releaseWaylandDisplay(wl_display* display) {
  if (display == nullptr) return;
  std::lock_guard<std::mutex> lock(gDisplayMutex);
  if (gSharedDisplay == display && gDisplayReferences > 0) --gDisplayReferences;
  // The connection deliberately outlives the last window. `Application`'s shared device was created
  // against this `wl_display` and its EGL display owns Wayland proxies; tearing the connection down
  // before the device made `destroyEglPlatformContext` walk a freed proxy array (SIGSEGV in
  // libnvidia-egl-wayland). One process-wide connection is the right lifetime for a desktop app.
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
    const std::string line = text.substr(start,
      end == std::string::npos ? std::string::npos : end - start);
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

Key keyFromXkb(const xkb_keysym_t symbol) {
  if (symbol >= XKB_KEY_a && symbol <= XKB_KEY_z) {
    return static_cast<Key>(static_cast<uint16_t>(Key::A) + static_cast<uint16_t>(symbol - XKB_KEY_a));
  }
  if (symbol >= XKB_KEY_A && symbol <= XKB_KEY_Z) {
    return static_cast<Key>(static_cast<uint16_t>(Key::A) + static_cast<uint16_t>(symbol - XKB_KEY_A));
  }
  if (symbol >= XKB_KEY_0 && symbol <= XKB_KEY_9) {
    return static_cast<Key>(static_cast<uint16_t>(Key::Digit0) +
      static_cast<uint16_t>(symbol - XKB_KEY_0));
  }
  if (symbol >= XKB_KEY_F1 && symbol <= XKB_KEY_F12) {
    return static_cast<Key>(static_cast<uint16_t>(Key::F1) +
      static_cast<uint16_t>(symbol - XKB_KEY_F1));
  }
  switch (symbol) {
    case XKB_KEY_Escape: return Key::Escape;
    case XKB_KEY_Return: return Key::Enter;
    case XKB_KEY_space: return Key::Space;
    case XKB_KEY_Tab: return Key::Tab;
    case XKB_KEY_BackSpace: return Key::Backspace;
    case XKB_KEY_Left: return Key::Left;
    case XKB_KEY_Right: return Key::Right;
    case XKB_KEY_Up: return Key::Up;
    case XKB_KEY_Down: return Key::Down;
    case XKB_KEY_Delete: return Key::Delete;
    case XKB_KEY_Home: return Key::Home;
    case XKB_KEY_End: return Key::End;
    case XKB_KEY_Prior: return Key::PageUp;
    case XKB_KEY_Next: return Key::PageDown;
    case XKB_KEY_Insert: return Key::Insert;
    case XKB_KEY_comma: return Key::Comma;
    case XKB_KEY_period: return Key::Period;
    case XKB_KEY_slash: return Key::Slash;
    case XKB_KEY_backslash: return Key::Backslash;
    case XKB_KEY_semicolon: return Key::Semicolon;
    case XKB_KEY_apostrophe: return Key::Apostrophe;
    case XKB_KEY_bracketleft: return Key::LeftBracket;
    case XKB_KEY_bracketright: return Key::RightBracket;
    case XKB_KEY_minus: return Key::Minus;
    case XKB_KEY_equal: return Key::Equal;
    case XKB_KEY_grave: return Key::Grave;
    case XKB_KEY_Shift_L: return Key::LeftShift;
    case XKB_KEY_Shift_R: return Key::RightShift;
    case XKB_KEY_Control_L: return Key::LeftControl;
    case XKB_KEY_Control_R: return Key::RightControl;
    case XKB_KEY_Alt_L: return Key::LeftAlt;
    case XKB_KEY_Alt_R: return Key::RightAlt;
    case XKB_KEY_Meta_L: return Key::LeftMeta;
    case XKB_KEY_Meta_R: return Key::RightMeta;
    case XKB_KEY_Super_L: return Key::LeftMeta;
    case XKB_KEY_Super_R: return Key::RightMeta;
    case XKB_KEY_Caps_Lock: return Key::CapsLock;
    case XKB_KEY_Num_Lock: return Key::NumLock;
    case XKB_KEY_Scroll_Lock: return Key::ScrollLock;
    case XKB_KEY_Print: return Key::PrintScreen;
    case XKB_KEY_Pause: return Key::Pause;
    case XKB_KEY_Menu: return Key::Menu;
    case XKB_KEY_KP_0:
    case XKB_KEY_KP_1:
    case XKB_KEY_KP_2:
    case XKB_KEY_KP_3:
    case XKB_KEY_KP_4:
    case XKB_KEY_KP_5:
    case XKB_KEY_KP_6:
    case XKB_KEY_KP_7:
    case XKB_KEY_KP_8:
    case XKB_KEY_KP_9:
      return static_cast<Key>(static_cast<uint16_t>(Key::Numpad0) +
        static_cast<uint16_t>(symbol - XKB_KEY_KP_0));
    case XKB_KEY_KP_Multiply: return Key::NumpadMultiply;
    case XKB_KEY_KP_Add: return Key::NumpadAdd;
    case XKB_KEY_KP_Subtract: return Key::NumpadSubtract;
    case XKB_KEY_KP_Decimal: return Key::NumpadDecimal;
    case XKB_KEY_KP_Divide: return Key::NumpadDivide;
    case XKB_KEY_KP_Enter: return Key::NumpadEnter;
    case XKB_KEY_KP_Equal: return Key::NumpadEqual;
    default: return Key::Unknown;
  }
}

/**
 * The payload bytes a drag target asks for.
 *
 * Wayland carries MIME names, and the text aliases all mean "text/plain"; the secondary
 * representation of a payload answers under its own MIME, so a target that wants the plain-text view
 * of a private payload still gets it.
 */
std::string waylandDragData(const DragPayload& payload, const char* mime) {
  if (mime == nullptr) return {};
  std::string wanted = mime;
  if (wanted == "text/plain;charset=utf-8" || wanted == "UTF8_STRING" || wanted == "STRING") {
    wanted = "text/plain";
  }
  if (payload.type() == wanted) return payload.data();
  if (payload.hasSecondary() && payload.secondaryType() == wanted) return payload.secondaryData();
  return {};
}

/** The MIME names a drag source advertises for @p payload. */
std::vector<std::string> waylandDragMimes(const DragPayload& payload) {
  std::vector<std::string> mimes;
  const auto add = [&mimes](const std::string& mime) {
    if (mime.empty()) return;
    if (mime == "text/plain") {
      mimes.emplace_back("text/plain;charset=utf-8");
      mimes.emplace_back("text/plain");
      return;
    }
    if (std::find(mimes.begin(), mimes.end(), mime) == mimes.end()) mimes.push_back(mime);
  };
  add(payload.type());
  if (payload.hasSecondary()) add(payload.secondaryType());
  return mimes;
}

/** One blocking Wayland drag; the listener writes into this while `start_drag` runs. */
struct WaylandDragState {
  const DragPayload* payload = nullptr;
  bool finished = false;
  bool cancelled = false;
};

void dragSourceSend(void* data, wl_data_source*, const char* mime, const int32_t fd) {
  auto* state = static_cast<WaylandDragState*>(data);
  const std::string bytes =
    state != nullptr && state->payload != nullptr ? waylandDragData(*state->payload, mime)
                                                  : std::string{};
  size_t written = 0;
  while (written < bytes.size()) {
    const ssize_t count = ::write(fd, bytes.data() + written, bytes.size() - written);
    if (count <= 0) break;
    written += static_cast<size_t>(count);
  }
  ::close(fd);
}

void dragSourceCancelled(void* data, wl_data_source*) {
  static_cast<WaylandDragState*>(data)->cancelled = true;
}

void dragSourceFinished(void* data, wl_data_source*) {
  static_cast<WaylandDragState*>(data)->finished = true;
}

const wl_data_source_listener kDragSourceListener = {
  [](void*, wl_data_source*, const char*) {},
  dragSourceSend,
  dragSourceCancelled,
  [](void*, wl_data_source*) {},
  dragSourceFinished,
  [](void*, wl_data_source*, uint32_t) {},
};

} // namespace

void shutdownPlatformDisplay() {
  if (gSharedDisplay != nullptr && gDisplayReferences == 0) {
    wl_display_disconnect(gSharedDisplay);
    gSharedDisplay = nullptr;
  }
}

class WaylandPlatform final : public WindowPlatform {
 public:
  WaylandPlatform(Window::Options options, backend::Device* sharedDevice)
      : mOptions(std::move(options)), mSharedDevice(sharedDevice), mWidth(options.width),
        mHeight(options.height) {}

  ~WaylandPlatform() override {
    mSurface.reset();
    mWake.close();
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
    if (mTextInput != nullptr) zwp_text_input_v1_destroy(mTextInput);
    if (mTextInputManager != nullptr) zwp_text_input_manager_v1_destroy(mTextInputManager);
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
    if (mTextInputV3 != nullptr) zwp_text_input_v3_destroy(mTextInputV3);
    if (mTextInputManagerV3 != nullptr) zwp_text_input_manager_v3_destroy(mTextInputManagerV3);
#endif
    if (mDropOffer != nullptr) wl_data_offer_destroy(mDropOffer);
    if (mDataDevice != nullptr) wl_data_device_destroy(mDataDevice);
    if (mDataDeviceManager != nullptr) wl_data_device_manager_destroy(mDataDeviceManager);
#if defined(CORAL_UI_WAYLAND_ALPHA_MODIFIER)
    if (mAlphaModifierSurface != nullptr) wp_alpha_modifier_surface_v1_destroy(mAlphaModifierSurface);
    if (mAlphaModifier != nullptr) wp_alpha_modifier_v1_destroy(mAlphaModifier);
#endif
#if defined(CORAL_UI_WAYLAND_XDG_FOREIGN)
    if (mExported != nullptr) zxdg_exported_v2_destroy(mExported);
    if (mExporter != nullptr) zxdg_exporter_v2_destroy(mExporter);
#endif
#if defined(CORAL_UI_WAYLAND_XDG_DECORATION)
    if (mDecoration != nullptr) zxdg_toplevel_decoration_v1_destroy(mDecoration);
    if (mDecorationManager != nullptr) zxdg_decoration_manager_v1_destroy(mDecorationManager);
#endif
#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
    if (mCursorShapeDevice != nullptr) wp_cursor_shape_device_v1_destroy(mCursorShapeDevice);
    if (mCursorShapeManager != nullptr) wp_cursor_shape_manager_v1_destroy(mCursorShapeManager);
#endif
#if defined(CORAL_UI_WAYLAND_CURSOR_THEME)
    if (mCursorSurface != nullptr) wl_surface_destroy(mCursorSurface);
    if (mCursorTheme != nullptr) wl_cursor_theme_destroy(mCursorTheme);
#endif
    if (mShm != nullptr) wl_shm_destroy(mShm);
    if (mPointer != nullptr) wl_pointer_release(mPointer);
    if (mKeyboard != nullptr) wl_keyboard_release(mKeyboard);
    if (mSeat != nullptr) wl_seat_release(mSeat);
    for (OutputEntry& output: mOutputs) {
      if (output.output != nullptr) wl_output_release(output.output);
    }
#if defined(CORAL_UI_WAYLAND_FRACTIONAL_SCALE)
    if (mFractionalScale != nullptr) wp_fractional_scale_v1_destroy(mFractionalScale);
    if (mFractionalScaleManager != nullptr) {
      wp_fractional_scale_manager_v1_destroy(mFractionalScaleManager);
    }
#endif
#if defined(CORAL_UI_WAYLAND_VIEWPORTER)
    if (mViewport != nullptr) wp_viewport_destroy(mViewport);
    if (mViewporter != nullptr) wp_viewporter_destroy(mViewporter);
#endif
    if (mEglWindow != nullptr) wl_egl_window_destroy(mEglWindow);
    if (mToplevel != nullptr) xdg_toplevel_destroy(mToplevel);
    if (mXdgSurface != nullptr) xdg_surface_destroy(mXdgSurface);
    if (mWlSurface != nullptr) wl_surface_destroy(mWlSurface);
    if (mWmBase != nullptr) xdg_wm_base_destroy(mWmBase);
    if (mCompositor != nullptr) wl_compositor_destroy(mCompositor);
    if (mRegistry != nullptr) wl_registry_destroy(mRegistry);
    if (mXkbState != nullptr) xkb_state_unref(mXkbState);
    if (mXkbKeymap != nullptr) xkb_keymap_unref(mXkbKeymap);
    if (mXkbContext != nullptr) xkb_context_unref(mXkbContext);
    releaseWaylandDisplay(mDisplay);
  }

  bool initialize(std::string* diagnostics) {
    if (mOptions.renderHost == RenderHostKind::RenderThread) {
      // The wl_surface and its proxies belong to the UI thread's default event queue. Handing the
      // surface to the render thread makes libwayland touch proxies from two threads and crashes
      // (SIGSEGV), so reject the combination until a per-surface queue exists. `Auto` still falls
      // back to X11, which does support a window target on the render thread.
      if (diagnostics != nullptr) {
        *diagnostics =
          "threaded Wayland is not supported yet: the wl_surface is created on the UI thread and "
          "cannot be handed to the render thread; use the immediate render host";
      }
      return false;
    }
    mDisplay = acquireWaylandDisplay();
    if (mDisplay == nullptr) {
      if (diagnostics != nullptr) *diagnostics = "the Wayland display could not be opened";
      return false;
    }
    mWake.open();
    mRegistry = wl_display_get_registry(mDisplay);
    wl_registry_add_listener(mRegistry, &kRegistryListener, this);
    wl_display_roundtrip(mDisplay);
    if (mCompositor == nullptr || mWmBase == nullptr) {
      if (diagnostics != nullptr) *diagnostics = "the Wayland compositor or xdg_wm_base is missing";
      return false;
    }
    xdg_wm_base_add_listener(mWmBase, &kWmListener, this);
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
    if (mTextInputManagerV3 != nullptr && mSeat != nullptr) {
      mTextInputV3 = zwp_text_input_manager_v3_get_text_input(mTextInputManagerV3, mSeat);
      zwp_text_input_v3_add_listener(mTextInputV3, &kTextInputListenerV3, this);
      mImeProtocol = ImeProtocol::V3;
    }
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
    if (mImeProtocol == ImeProtocol::None && mTextInputManager != nullptr && mSeat != nullptr) {
      mTextInput = zwp_text_input_manager_v1_create_text_input(mTextInputManager);
      zwp_text_input_v1_add_listener(mTextInput, &kTextInputListener, this);
      mImeProtocol = ImeProtocol::V1;
    }
#endif
    if (mDataDeviceManager != nullptr && mSeat != nullptr) {
      mDataDevice = wl_data_device_manager_get_data_device(mDataDeviceManager, mSeat);
      wl_data_device_add_listener(mDataDevice, &kDataDeviceListener, this);
    }
    mWlSurface = wl_compositor_create_surface(mCompositor);
    if (mWlSurface == nullptr) {
      if (diagnostics != nullptr) *diagnostics = "the Wayland surface could not be created";
      return false;
    }
#if defined(CORAL_UI_WAYLAND_ALPHA_MODIFIER)
    if (mAlphaModifier != nullptr) {
      mAlphaModifierSurface = wp_alpha_modifier_v1_get_surface(mAlphaModifier, mWlSurface);
    }
#endif
    // EGL needs a wl_egl_window wrapping the role surface; Vulkan wants the wl_surface itself.
    // Without this the OpenGL window surface cannot be created and every native Wayland run fails.
    mEglWindow = wl_egl_window_create(mWlSurface, std::max(1, mWidth), std::max(1, mHeight));
    mSurfaceListener.enter = [](void* data, wl_surface*, wl_output* output) {
      auto* platform = static_cast<WaylandPlatform*>(data);
      if (std::find(platform->mEnteredOutputs.begin(), platform->mEnteredOutputs.end(), output) ==
          platform->mEnteredOutputs.end()) {
        platform->mEnteredOutputs.push_back(output);
        platform->applyOutputScale();
      }
    };
    mSurfaceListener.leave = [](void* data, wl_surface*, wl_output* output) {
      auto* platform = static_cast<WaylandPlatform*>(data);
      platform->mEnteredOutputs.erase(
        std::remove(platform->mEnteredOutputs.begin(), platform->mEnteredOutputs.end(), output),
        platform->mEnteredOutputs.end());
      platform->applyOutputScale();
    };
    mSurfaceListener.preferred_buffer_scale = [](void* data, wl_surface*, int32_t factor) {
      auto* platform = static_cast<WaylandPlatform*>(data);
      platform->setScale(static_cast<float>(factor));
    };
    mSurfaceListener.preferred_buffer_transform = [](void*, wl_surface*, uint32_t) {};
    wl_surface_add_listener(mWlSurface, &mSurfaceListener, this);
#if defined(CORAL_UI_WAYLAND_VIEWPORTER)
    if (mViewporter != nullptr) {
      mViewport = wp_viewporter_get_viewport(mViewporter, mWlSurface);
    }
#endif
#if defined(CORAL_UI_WAYLAND_FRACTIONAL_SCALE)
    if (mFractionalScaleManager != nullptr) {
      mFractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale(
        mFractionalScaleManager, mWlSurface);
      wp_fractional_scale_v1_add_listener(mFractionalScale, &kFractionalScaleListener, this);
    }
#endif
    mXdgSurface = xdg_wm_base_get_xdg_surface(mWmBase, mWlSurface);
    xdg_surface_add_listener(mXdgSurface, &kXdgSurfaceListener, this);
    mToplevel = xdg_surface_get_toplevel(mXdgSurface);
    xdg_toplevel_add_listener(mToplevel, &kToplevelListener, this);
#if defined(CORAL_UI_WAYLAND_XDG_DECORATION)
    if (mDecorationManager != nullptr) {
      mDecoration = zxdg_decoration_manager_v1_get_toplevel_decoration(mDecorationManager, mToplevel);
      static const zxdg_toplevel_decoration_v1_listener kDecorationListener = {
        [](void* data, zxdg_toplevel_decoration_v1*, uint32_t mode) {
          static_cast<WaylandPlatform*>(data)->mDecorationMode = mode;
        },
      };
      zxdg_toplevel_decoration_v1_add_listener(mDecoration, &kDecorationListener, this);
      // A window starts system-decorated; `Window::setDecorations(Client)` flips it after creation.
      zxdg_toplevel_decoration_v1_set_mode(mDecoration,
        ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
#endif
    xdg_toplevel_set_title(mToplevel, mOptions.title.c_str());
    xdg_toplevel_set_app_id(mToplevel, mOptions.appId.c_str());
    wl_surface_commit(mWlSurface);
    wl_display_roundtrip(mDisplay);
    if (!mConfigured) wl_display_roundtrip(mDisplay);
    if (mWidth <= 0 || mHeight <= 0) {
      mWidth = mOptions.width;
      mHeight = mOptions.height;
    }
    if (mLogicalWidth <= 0 || mLogicalHeight <= 0) {
      mLogicalWidth = std::max(1, static_cast<int>(std::lround(mWidth / mScale)));
      mLogicalHeight = std::max(1, static_cast<int>(std::lround(mHeight / mScale)));
    }
    // The roundtrip above may already have delivered `fractional_scale.preferred_scale`; using the
    // integer scale here would override the fractional value and make a 1.75-scale buffer invalid.
    wl_surface_set_buffer_scale(mWlSurface,
      mFractionalScaleActive ? 1 : std::max(1, static_cast<int>(std::lround(mScale))));
    if (mOptions.renderHost != RenderHostKind::RenderThread && !createSurface()) {
      if (diagnostics != nullptr) *diagnostics = "the GPU Wayland window surface could not be created";
      return false;
    }
    return true;
  }

  [[nodiscard]] Window::NativePlatform kind() const override {
    return Window::NativePlatform::Wayland;
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
    return mScale;
  }

  bool pump(WindowHost& host) override {
#if defined(CORAL_UI_HAS_ATSPI)
    if (mAccessibility) mAccessibility->pump();
#endif
    mHost = &host;
    if (mDisplay == nullptr) return false;
    while (wl_display_prepare_read(mDisplay) != 0) {
      if (wl_display_dispatch_pending(mDisplay) < 0) return false;
    }
    wl_display_flush(mDisplay);
    pollfd descriptor{ wl_display_get_fd(mDisplay), POLLIN, 0 };
    if (poll(&descriptor, 1, 0) > 0) {
      if (wl_display_read_events(mDisplay) < 0) return false;
    } else {
      wl_display_cancel_read(mDisplay);
    }
    return wl_display_dispatch_pending(mDisplay) >= 0;
  }

  void waitForEvents(const int timeoutMs) override {
    if (mDisplay == nullptr) return;
    // Flush first so a request made by another thread reaches the compositor, then wait for the
    // socket or the wake pipe. `pump` owns the prepare/read pair, so this only polls.
    wl_display_flush(mDisplay);
    pollfd descriptors[2]{
      { wl_display_get_fd(mDisplay), POLLIN, 0 },
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
    graphics::Canvas* canvas = mSurface->beginFrame();
    if (canvas == nullptr) mDiagnostics = mSurface->diagnostics();
    return canvas;
  }
  bool present() override {
    if (mSurface == nullptr) return false;
    const bool presented = mSurface->present();
    if (!presented) mDiagnostics = mSurface->diagnostics();
    return presented;
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
    if (mOptions.renderHost != RenderHostKind::RenderThread &&
        (mSurface == nullptr || !mSurface->resize(width, height))) {
      mDiagnostics = mSurface != nullptr ? mSurface->diagnostics() : "no window surface";
      return false;
    }
    mWidth = static_cast<int>(width);
    mHeight = static_cast<int>(height);
    resizeEglWindow();
    mLogicalWidth = std::max(1, static_cast<int>(std::lround(width / mScale)));
    mLogicalHeight = std::max(1, static_cast<int>(std::lround(height / mScale)));
    updateViewport();
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
    native.kind = graphics::gpu::NativeDisplayKind::Wayland;
    native.display = mDisplay;
    native.surface = mWlSurface;
    return native;
  }

  void setDecorations(const Window::Decorations decorations) override {
#if defined(CORAL_UI_WAYLAND_XDG_DECORATION)
    if (mDecoration != nullptr) {
      zxdg_toplevel_decoration_v1_set_mode(mDecoration,
        decorations == Window::Decorations::Client
          ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE
          : ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
      wl_display_flush(mDisplay);
    }
#else
    (void)decorations;
#endif
  }

  void setOpacity(const float opacity) override {
#if defined(CORAL_UI_WAYLAND_ALPHA_MODIFIER)
    if (mAlphaModifierSurface != nullptr) {
      const auto multiplier = static_cast<uint32_t>(
        std::clamp(opacity, 0.0f, 1.0f) * 4294967295.0f);
      wp_alpha_modifier_surface_v1_set_multiplier(mAlphaModifierSurface, multiplier);
      wl_display_flush(mDisplay);
    }
#else
    (void)opacity;
#endif
  }

  void setAlwaysOnTop(bool onTop) override { (void)onTop; }

  void setCursor(const Window::Cursor cursor) override {
    mCursor = cursor;
#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
    if (!useThemeCursor()) {
      applyCursorShape(cursor);
      return;
    }
#endif
#if defined(CORAL_UI_WAYLAND_CURSOR_THEME)
    applyThemeCursor();
#endif
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
    if (mToplevel == nullptr) return;
    const std::string text(title);
    xdg_toplevel_set_title(mToplevel, text.c_str());
    wl_display_flush(mDisplay);
  }

  void setMinimumSize(const int width, const int height) override {
    if (mToplevel == nullptr) return;
    // `Window::setMinimumSize` is in logical units but xdg_toplevel wants surface-local pixels; on a
    // HiDPI output the unscaled value made the minimum twice as large as requested.
    const auto toSurface = [this](const int value) {
      return std::max(0, static_cast<int>(std::lround(static_cast<float>(value) * mScale)));
    };
    xdg_toplevel_set_min_size(mToplevel, toSurface(width), toSurface(height));
    wl_display_flush(mDisplay);
  }

  void setFullscreen(const bool fullscreen) override {
    if (mToplevel == nullptr) return;
    if (fullscreen) {
      xdg_toplevel_set_fullscreen(mToplevel, nullptr);
    } else {
      xdg_toplevel_unset_fullscreen(mToplevel);
    }
    wl_display_flush(mDisplay);
  }

  void setTransientFor(WindowPlatform* parent) override {
    if (mToplevel == nullptr) return;
    auto* wayland = dynamic_cast<WaylandPlatform*>(parent);
    xdg_toplevel_set_parent(mToplevel, wayland != nullptr ? wayland->mToplevel : nullptr);
    wl_display_flush(mDisplay);
  }

  void setMaximized(const bool maximized) override {
    if (mToplevel == nullptr) return;
    if (maximized) {
      xdg_toplevel_set_maximized(mToplevel);
    } else {
      xdg_toplevel_unset_maximized(mToplevel);
    }
    wl_display_flush(mDisplay);
  }

  void setMinimized(const bool minimized) override {
    // xdg-shell has no "unminimize": the compositor (or the user) restores a minimized window.
    if (mToplevel == nullptr || !minimized) return;
    xdg_toplevel_set_minimized(mToplevel);
    wl_display_flush(mDisplay);
  }

  // Wayland gives clients no global window position; the compositor owns placement.
  void setPosition(int, int) override {}

  [[nodiscard]] Rect screenBounds() const override {
    return Rect::MakeLTRB(0.0f, 0.0f, static_cast<float>(mWidth), static_cast<float>(mHeight));
  }

  /** The wl_surface a render thread uses to create its window surface. */
  [[nodiscard]] void* nativeWindow() const override { return mWlSurface; }

  [[nodiscard]] std::string portalParentHandle() const override {
#if defined(CORAL_UI_WAYLAND_XDG_FOREIGN)
    if (!mExportedHandle.empty()) return "wayland:" + mExportedHandle;
    if (mExporter == nullptr || mWlSurface == nullptr) return {};
    static const zxdg_exported_v2_listener kExportedListener = {
      [](void* data, zxdg_exported_v2*, const char* handle) {
        if (handle != nullptr) static_cast<WaylandPlatform*>(data)->mExportedHandle = handle;
      },
    };
    auto* self = const_cast<WaylandPlatform*>(this);
    self->mExported = zxdg_exporter_v2_export_toplevel(self->mExporter, self->mWlSurface);
    if (self->mExported == nullptr) return {};
    zxdg_exported_v2_add_listener(self->mExported, &kExportedListener, self);
    wl_display_flush(self->mDisplay);
    // The handle arrives in the next batch; a roundtrip is cheap and makes the value usable by the
    // portal call that follows immediately.
    wl_display_roundtrip(self->mDisplay);
    return self->mExportedHandle.empty() ? std::string{} : "wayland:" + self->mExportedHandle;
#else
    return {};
#endif
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

  void onFocusChanged(Layer* focused) override {
    updateTextInput(focused);
  }

  void clearDropOffer() {
    if (mDropOffer != nullptr) {
      wl_data_offer_destroy(mDropOffer);
      mDropOffer = nullptr;
    }
    mDropMime.clear();
  }

  void readDrop() {
    if (mDropOffer == nullptr || mHost == nullptr) {
      clearDropOffer();
      return;
    }
    int fds[2] = { -1, -1 };
    if (pipe(fds) != 0) {
      clearDropOffer();
      return;
    }
    const std::string mime = mDropMime.empty() ? "text/uri-list" : mDropMime;
    wl_data_offer_receive(mDropOffer, mime.c_str(), fds[1]);
    close(fds[1]);
    wl_display_flush(mDisplay);
    std::string text;
    char buffer[4096];
    for (;;) {
      const ssize_t count = read(fds[0], buffer, sizeof(buffer));
      if (count <= 0) break;
      text.append(buffer, static_cast<size_t>(count));
    }
    close(fds[0]);
    const std::vector<std::string> paths = parseUriList(text);
    clearDropOffer();
    if (!paths.empty()) {
      DropEvent event;
      event.position = mDropPosition;
      event.paths = paths;
      mHost->dispatchDrop(event);
    }
  }

  void updateImeCursorRect(const Rect& rect) override {
    mImeCursorRect = rect;
    if (!mTextInputActive) {
      return;
    }
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
    if (mImeProtocol == ImeProtocol::V3 && mTextInputV3 != nullptr) {
      sendCursorRectV3();
      refreshSurroundingText();
      return;
    }
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
    if (mImeProtocol == ImeProtocol::V1 && mTextInput != nullptr) {
      sendCursorRectV1();
    }
#endif
    refreshSurroundingText();
  }

  /**
   * Hands the drag to the compositor.
   *
   * `wl_data_device.start_drag` needs the serial of the pointer button that started the gesture, so
   * this only works from a press/move handler (which is where `Layer::setDragOutProvider` lands). The
   * call blocks until the compositor reports the drop (`dnd_finished`) or a cancel; the nested
   * dispatch is the Wayland counterpart of OLE's message pump inside `DoDragDrop`, and the window's
   * own listeners keep receiving pointer events while it runs.
   */
  DragEffect beginDragOut(const DragPayload& payload, const DragEffect allowed) override {
    if (allowed != DragEffect::Copy) return kNoDragEffect;
    if (mDataDevice == nullptr || mDataDeviceManager == nullptr || mWlSurface == nullptr ||
        mPointerButtonSerial == 0) {
      return kNoDragEffect;
    }
    wl_data_source* source = wl_data_device_manager_create_data_source(mDataDeviceManager);
    if (source == nullptr) return kNoDragEffect;
    WaylandDragState state;
    state.payload = &payload;
    wl_data_source_add_listener(source, &kDragSourceListener, &state);
    for (const std::string& mime: waylandDragMimes(payload)) {
      wl_data_source_offer(source, mime.c_str());
    }
    wl_data_device_start_drag(mDataDevice, source, mWlSurface, nullptr, mPointerButtonSerial);
    wl_display_flush(mDisplay);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!state.finished && !state.cancelled &&
        std::chrono::steady_clock::now() < deadline) {
      while (wl_display_prepare_read(mDisplay) != 0) {
        wl_display_dispatch_pending(mDisplay);
      }
      wl_display_flush(mDisplay);
      pollfd descriptor{ wl_display_get_fd(mDisplay), POLLIN, 0 };
      const int ready = poll(&descriptor, 1, 50);
      if (ready > 0 && (descriptor.revents & POLLIN) != 0) {
        wl_display_read_events(mDisplay);
        wl_display_dispatch_pending(mDisplay);
      } else {
        wl_display_cancel_read(mDisplay);
      }
    }
    // The client owns the source for the whole drag, so it destroys it when the drag ends either way.
    wl_data_source_destroy(source);
    return state.finished ? DragEffect::Copy : kNoDragEffect;
  }

  // Wayland callbacks need access to the object state; this is an internal type so the fields stay
  // public rather than scattering friend declarations.
  std::string mTitle = "Coral UI";
#if defined(CORAL_UI_HAS_ATSPI)
  std::unique_ptr<detail::AtspiBridge> mAccessibility;
#endif
  wl_display* mDisplay = nullptr;
  WakePipe mWake;
  wl_registry* mRegistry = nullptr;
  wl_compositor* mCompositor = nullptr;
  struct OutputEntry {
    wl_output* output = nullptr;
    int32_t scale = 1;
  };
  std::vector<OutputEntry> mOutputs;
  std::vector<wl_output*> mEnteredOutputs;
  wl_output_listener mOutputListener{};
  wl_surface_listener mSurfaceListener{};
  float mScale = 1.0f;
  int mLogicalWidth = 0;
  int mLogicalHeight = 0;
#if defined(CORAL_UI_WAYLAND_FRACTIONAL_SCALE)
  wp_fractional_scale_manager_v1* mFractionalScaleManager = nullptr;
  wp_fractional_scale_v1* mFractionalScale = nullptr;
#endif
#if defined(CORAL_UI_WAYLAND_VIEWPORTER)
  wp_viewporter* mViewporter = nullptr;
  wp_viewport* mViewport = nullptr;
#endif
  bool mFractionalScaleActive = false;

#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
  /** Maps the public cursor to the compositor-side shape enum and applies it. */
  void applyCursorShape(const Window::Cursor cursor) {
    if (mCursorShapeDevice == nullptr) return;
    uint32_t shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    switch (cursor) {
      case Window::Cursor::IBeam: shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT; break;
      case Window::Cursor::Hand: shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER; break;
      case Window::Cursor::ResizeHorizontal: shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE; break;
      case Window::Cursor::ResizeVertical: shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE; break;
      case Window::Cursor::Move: shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE; break;
      case Window::Cursor::Crosshair: shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR; break;
      case Window::Cursor::NotAllowed: shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED; break;
      case Window::Cursor::Wait: shape = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT; break;
      case Window::Cursor::Arrow:
      default: break;
    }
    wp_cursor_shape_device_v1_set_shape(mCursorShapeDevice, mPointerEnterSerial, shape);
    wl_display_flush(mDisplay);
  }
#endif

  /**
   * Chooses the integer buffer scale from the outputs the surface currently covers.
   *
   * `preferred_buffer_scale` is the compositor's primary signal; this is the fallback for compositors
   * that only send `wl_output.scale`, and it is what makes moving between a 1x and a 2x output pick
   * the right scale instead of keeping the first output's value forever.
   */
  void applyOutputScale() {
    if (mFractionalScaleActive) return;
    int32_t chosen = 1;
    for (wl_output* output: mEnteredOutputs) {
      for (const OutputEntry& entry: mOutputs) {
        if (entry.output == output) chosen = std::max(chosen, entry.scale);
      }
    }
    setScale(static_cast<float>(chosen));
  }

#if defined(CORAL_UI_WAYLAND_CURSOR_THEME)
  bool useThemeCursor() const {
#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
    if (std::getenv("CORAL_UI_CURSOR_THEME") != nullptr) return true;
    return mCursorShapeDevice == nullptr;
#else
    return true;
#endif
  }

  static const char* cursorName(const Window::Cursor cursor) {
    switch (cursor) {
      case Window::Cursor::IBeam: return "text";
      case Window::Cursor::Hand: return "pointer";
      case Window::Cursor::ResizeHorizontal: return "ew-resize";
      case Window::Cursor::ResizeVertical: return "ns-resize";
      case Window::Cursor::Move: return "move";
      case Window::Cursor::Crosshair: return "crosshair";
      case Window::Cursor::NotAllowed: return "not-allowed";
      case Window::Cursor::Wait: return "wait";
      case Window::Cursor::Arrow:
      default: return "left_ptr";
    }
  }

  void applyThemeCursor() {
    if (!useThemeCursor() || mPointer == nullptr || mShm == nullptr || mPointerEnterSerial == 0) return;
    if (mCursorTheme == nullptr) {
      const int size = std::max(16, static_cast<int>(std::lround(24.0f * mScale)));
      mCursorTheme = wl_cursor_theme_load(nullptr, size, mShm);
    }
    if (mCursorTheme == nullptr) return;
    wl_cursor* cursor = wl_cursor_theme_get_cursor(mCursorTheme, cursorName(mCursor));
    if (cursor == nullptr || cursor->image_count == 0) return;
    const wl_cursor_image* image = cursor->images[0];
    if (mCursorSurface != nullptr) wl_surface_destroy(mCursorSurface);
    mCursorSurface = wl_compositor_create_surface(mCompositor);
    if (mCursorSurface == nullptr) return;
    wl_surface_attach(mCursorSurface, wl_cursor_image_get_buffer(
      const_cast<wl_cursor_image*>(image)), 0, 0);
    wl_surface_commit(mCursorSurface);
    wl_pointer_set_cursor(mPointer, mPointerEnterSerial, mCursorSurface, image->hotspot_x,
      image->hotspot_y);
    wl_display_flush(mDisplay);
  }
#endif

  /** Keeps the EGL window in step with the surface's physical size (buffer scale aware). */
  void resizeEglWindow() {
    if (mEglWindow != nullptr) {
      wl_egl_window_resize(mEglWindow, std::max(1, mWidth), std::max(1, mHeight), 0, 0);
    }
  }

  void updateViewport() {
#if defined(CORAL_UI_WAYLAND_VIEWPORTER)
    if (mViewport == nullptr) return;
    if (mFractionalScaleActive && mLogicalWidth > 0 && mLogicalHeight > 0) {
      wp_viewport_set_destination(mViewport, mLogicalWidth, mLogicalHeight);
    } else {
      wp_viewport_set_destination(mViewport, -1, -1);
    }
#endif
  }

  void setScale(const float scale, const bool fractional = false) {
    if (!fractional && mFractionalScaleActive) {
      return;
    }
    const bool canFractional = fractional
#if defined(CORAL_UI_WAYLAND_VIEWPORTER)
      && mViewport != nullptr
#endif
      ;
    const float next = std::clamp(scale, 0.5f, 4.0f);
    if (std::fabs(next - mScale) < 0.001f && mFractionalScaleActive == canFractional) {
      return;
    }
    mScale = canFractional ? next : std::max(1.0f, std::round(next));
    mFractionalScaleActive = canFractional;
    if (mWlSurface != nullptr) {
      wl_surface_set_buffer_scale(mWlSurface,
        mFractionalScaleActive ? 1 : std::max(1, static_cast<int>(std::lround(mScale))));
    }
    updateViewport();
    if (mLogicalWidth > 0 && mLogicalHeight > 0) {
      const int physicalWidth = std::max(1, static_cast<int>(std::lround(mLogicalWidth * mScale)));
      const int physicalHeight = std::max(1, static_cast<int>(std::lround(mLogicalHeight * mScale)));
      mWidth = physicalWidth;
      mHeight = physicalHeight;
      resizeEglWindow();
      if (mSurface != nullptr) {
        mSurface->resize(static_cast<uint32_t>(physicalWidth), static_cast<uint32_t>(physicalHeight));
        if (mHost != nullptr) mHost->platformResized(physicalWidth, physicalHeight);
      }
    }
  }
  wl_seat* mSeat = nullptr;
  wl_data_device_manager* mDataDeviceManager = nullptr;
  wl_data_device* mDataDevice = nullptr;
  wl_data_offer* mDropOffer = nullptr;
  std::string mDropMime;
  Point mDropPosition{};
  wl_pointer* mPointer = nullptr;
  uint32_t mPointerEnterSerial = 0;
  Window::Cursor mCursor = Window::Cursor::Arrow;
#if defined(CORAL_UI_WAYLAND_ALPHA_MODIFIER)
  wp_alpha_modifier_v1* mAlphaModifier = nullptr;
  wp_alpha_modifier_surface_v1* mAlphaModifierSurface = nullptr;
#endif
#if defined(CORAL_UI_WAYLAND_XDG_FOREIGN)
  zxdg_exporter_v2* mExporter = nullptr;
  zxdg_exported_v2* mExported = nullptr;
  std::string mExportedHandle;
#endif
#if defined(CORAL_UI_WAYLAND_XDG_DECORATION)
  zxdg_decoration_manager_v1* mDecorationManager = nullptr;
  zxdg_toplevel_decoration_v1* mDecoration = nullptr;
  uint32_t mDecorationMode = 0;
#endif
#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
  wp_cursor_shape_manager_v1* mCursorShapeManager = nullptr;
  wp_cursor_shape_device_v1* mCursorShapeDevice = nullptr;
#endif
#if defined(CORAL_UI_WAYLAND_CURSOR_THEME)
  wl_cursor_theme* mCursorTheme = nullptr;
  wl_surface* mCursorSurface = nullptr;
#endif
  wl_keyboard* mKeyboard = nullptr;
  wl_surface* mWlSurface = nullptr;
  wl_egl_window* mEglWindow = nullptr;
  wl_shm* mShm = nullptr;
  xdg_wm_base* mWmBase = nullptr;
  xdg_surface* mXdgSurface = nullptr;
  xdg_toplevel* mToplevel = nullptr;
  xkb_context* mXkbContext = nullptr;
  xkb_keymap* mXkbKeymap = nullptr;
  xkb_state* mXkbState = nullptr;
  bool mConfigured = false;
  bool mMaximized = false;
  bool mMinimized = false;
  bool mFullscreen = false;
  bool mActivated = false;
  enum class ImeProtocol { None, V1, V3 };
  ImeProtocol mImeProtocol = ImeProtocol::None;
  bool mTextInputActive = false;
  bool mWantsTextInput = false;
  std::string mLastSurroundingText;
  size_t mLastSurroundingCursor = 0;
  size_t mLastSurroundingAnchor = 0;
  uint32_t mKeyboardEnterSerial = 0;
  /** The serial of the last pointer-button press; `start_drag` requires it. */
  uint32_t mPointerButtonSerial = 0;
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
  zwp_text_input_manager_v1* mTextInputManager = nullptr;
  zwp_text_input_v1* mTextInput = nullptr;
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
  zwp_text_input_manager_v3* mTextInputManagerV3 = nullptr;
  zwp_text_input_v3* mTextInputV3 = nullptr;
#endif
  WindowHost* mHost = nullptr;
  Point mPointerPosition{};
  Rect mImeCursorRect{};

  void sendCursorRectV3() {
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
    const int x = static_cast<int>(std::lround(mImeCursorRect.getLeft()));
    const int y = static_cast<int>(std::lround(mImeCursorRect.getTop()));
    const int width = std::max(1, static_cast<int>(std::lround(mImeCursorRect.getWidth())));
    const int height = std::max(1, static_cast<int>(std::lround(mImeCursorRect.getHeight())));
    zwp_text_input_v3_set_cursor_rectangle(mTextInputV3, x, y, width, height);
    zwp_text_input_v3_commit(mTextInputV3);
#endif
  }

  void sendCursorRectV1() {
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
    const int x = static_cast<int>(std::lround(mImeCursorRect.getLeft()));
    const int y = static_cast<int>(std::lround(mImeCursorRect.getTop()));
    const int width = std::max(1, static_cast<int>(std::lround(mImeCursorRect.getWidth())));
    const int height = std::max(1, static_cast<int>(std::lround(mImeCursorRect.getHeight())));
    zwp_text_input_v1_set_cursor_rectangle(mTextInput, x, y, width, height);
#endif
  }

  void updateTextInput(Layer* focused) {
    bool accepts = false;
    for (Layer* layer = focused; layer != nullptr; layer = layer->parent()) {
      if (layer->acceptsTextInput()) {
        accepts = true;
        break;
      }
    }
    mWantsTextInput = accepts;
    if (!accepts) {
      if (mTextInputActive) deactivateTextInput();
      return;
    }
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
    if (mImeProtocol == ImeProtocol::V3) {
      if (!mTextInputActive && mTextInputV3 != nullptr && mKeyboardEnterSerial != 0) {
        zwp_text_input_v3_enable(mTextInputV3);
        sendCursorRectV3();
        refreshSurroundingText(true);
        mTextInputActive = true;
      }
      return;
    }
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
    if (mImeProtocol == ImeProtocol::V1 && !mTextInputActive && mTextInput != nullptr &&
        mSeat != nullptr && mWlSurface != nullptr) {
      zwp_text_input_v1_activate(mTextInput, mSeat, mWlSurface);
      sendCursorRectV1();
      refreshSurroundingText(true);
      zwp_text_input_v1_show_input_panel(mTextInput);
      mTextInputActive = true;
    }
#endif
  }

  void deactivateTextInput() {
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
    if (mImeProtocol == ImeProtocol::V3 && mTextInputV3 != nullptr) {
      zwp_text_input_v3_disable(mTextInputV3);
      zwp_text_input_v3_commit(mTextInputV3);
    }
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
    if (mImeProtocol == ImeProtocol::V1 && mTextInput != nullptr) {
      zwp_text_input_v1_deactivate(mTextInput, mSeat);
      zwp_text_input_v1_hide_input_panel(mTextInput);
    }
#endif
    mTextInputActive = false;
    mLastSurroundingText.clear();
    mLastSurroundingCursor = 0;
    mLastSurroundingAnchor = 0;
  }

 private:
  /**
   * Pushes the focused field's text/caret to the input method.
   *
   * Wayland counts characters, `TextInput` stores UTF-8 bytes, and an IME that only sees an empty
   * surrounding string cannot implement delete-surrounding or segment the text around the caret.
   */
  void refreshSurroundingText(const bool force = false) {
    if (mHost == nullptr) return;
    std::string text;
    size_t cursor = 0;
    size_t anchor = 0;
    if (!mHost->surroundingText(&text, &cursor, &anchor)) {
      text.clear();
      cursor = 0;
      anchor = 0;
    }
    const size_t cursorChars = utf8CharacterCount(text, cursor);
    const size_t anchorChars = utf8CharacterCount(text, anchor);
    if (!force && text == mLastSurroundingText && cursorChars == mLastSurroundingCursor &&
        anchorChars == mLastSurroundingAnchor) {
      return;
    }
    mLastSurroundingText = text;
    mLastSurroundingCursor = cursorChars;
    mLastSurroundingAnchor = anchorChars;
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
    if (mImeProtocol == ImeProtocol::V3 && mTextInputV3 != nullptr) {
      zwp_text_input_v3_set_surrounding_text(mTextInputV3, text.c_str(),
        static_cast<uint32_t>(cursorChars), static_cast<uint32_t>(anchorChars));
      zwp_text_input_v3_commit(mTextInputV3);
      return;
    }
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
    if (mImeProtocol == ImeProtocol::V1 && mTextInput != nullptr) {
      zwp_text_input_v1_set_surrounding_text(mTextInput, text.c_str(),
        static_cast<uint32_t>(cursorChars), static_cast<uint32_t>(anchorChars));
    }
#else
    (void)force;
#endif
  }

  /** Converts an IME's character-counted delete-surrounding range into byte offsets. */
  void dispatchDeleteCharacters(const int32_t before, const int32_t after) {
    if (mHost == nullptr) return;
    std::string text;
    size_t cursorByte = 0;
    size_t anchorByte = 0;
    if (!mHost->surroundingText(&text, &cursorByte, &anchorByte)) {
      if (before > 0) mHost->dispatchDelete(-before, static_cast<uint32_t>(before));
      if (after > 0) mHost->dispatchDelete(0, static_cast<uint32_t>(after));
      return;
    }
    const size_t cursorChars = utf8CharacterCount(text, cursorByte);
    if (before > 0) {
      const size_t startChar = cursorChars > static_cast<size_t>(before)
        ? cursorChars - static_cast<size_t>(before) : 0;
      const size_t startByte = utf8ByteOffset(text, startChar);
      mHost->dispatchDelete(static_cast<int32_t>(startByte) - static_cast<int32_t>(cursorByte),
        static_cast<uint32_t>(cursorByte - startByte));
    }
    if (after > 0) {
      const size_t endByte = utf8ByteOffset(text, cursorChars + static_cast<size_t>(after));
      mHost->dispatchDelete(0, static_cast<uint32_t>(endByte - cursorByte));
    }
  }

  bool createSurface() {
    graphics::gpu::WindowSurface::Configuration configuration;
  configuration.hdrOutput = mOptions.hdrOutput;
    configuration.nativeWindow = mWlSurface;
    configuration.nativeWindowExtra = mEglWindow;
    configuration.nativeDisplay = mDisplay;
    configuration.nativeWindowSystem = backend::NativeWindowSystem::Wayland;
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

  Window::Options mOptions;
  backend::Device* mSharedDevice = nullptr;
  int mWidth = 0;
  int mHeight = 0;
  std::unique_ptr<graphics::gpu::WindowSurface> mSurface;
  std::string mDiagnostics;

  static void registryGlobal(void* data, wl_registry* registry, uint32_t name,
      const char* interface, uint32_t version);
  static void registryRemove(void*, wl_registry*, uint32_t) {}
  static const wl_registry_listener kRegistryListener;
  static void seatCapabilities(void* data, wl_seat* seat, uint32_t capabilities);
  static const wl_seat_listener kSeatListener;
  static const wl_pointer_listener kPointerListener;
  static const wl_keyboard_listener kKeyboardListener;
  static void wmPing(void*, xdg_wm_base* base, uint32_t serial);
  static const xdg_wm_base_listener kWmListener;
  static void xdgConfigure(void* data, xdg_surface* surface, uint32_t serial);
  static const xdg_surface_listener kXdgSurfaceListener;
  static void toplevelConfigure(void* data, xdg_toplevel*, int32_t width, int32_t height,
    wl_array*);
  static void toplevelClose(void* data, xdg_toplevel*);
  static void toplevelConfigureBounds(void*, xdg_toplevel*, int32_t, int32_t) {}
  static void toplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}
  static const xdg_toplevel_listener kToplevelListener;
#if defined(CORAL_UI_WAYLAND_FRACTIONAL_SCALE)
  static void fractionalPreferred(void* data, wp_fractional_scale_v1*, uint32_t scale);
  static const wp_fractional_scale_v1_listener kFractionalScaleListener;
#endif
  static const wl_data_device_listener kDataDeviceListener;
  static const wl_data_offer_listener kDropOfferListener;
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
  static const zwp_text_input_v1_listener kTextInputListener;
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
  static const zwp_text_input_v3_listener kTextInputListenerV3;
#endif
};

const wl_registry_listener WaylandPlatform::kRegistryListener = {
  [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    WaylandPlatform::registryGlobal(data, registry, name, interface, version);
  },
  WaylandPlatform::registryRemove,
};

void WaylandPlatform::registryGlobal(void* data, wl_registry* registry, uint32_t name,
    const char* interface, uint32_t version) {
  auto* platform = static_cast<WaylandPlatform*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    platform->mCompositor = static_cast<wl_compositor*>(
      wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    platform->mWmBase = static_cast<xdg_wm_base*>(
      wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
  } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
    platform->mShm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    platform->mSeat = static_cast<wl_seat*>(
      wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 5u)));
    wl_seat_add_listener(platform->mSeat, &kSeatListener, platform);
  } else if (std::strcmp(interface, wl_data_device_manager_interface.name) == 0) {
    platform->mDataDeviceManager = static_cast<wl_data_device_manager*>(
      wl_registry_bind(registry, name, &wl_data_device_manager_interface, std::min(version, 3u)));
#if defined(CORAL_UI_WAYLAND_VIEWPORTER)
  } else if (std::strcmp(interface, wp_viewporter_interface.name) == 0) {
    platform->mViewporter = static_cast<wp_viewporter*>(
      wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
#endif
#if defined(CORAL_UI_WAYLAND_ALPHA_MODIFIER)
  } else if (std::strcmp(interface, wp_alpha_modifier_v1_interface.name) == 0) {
    platform->mAlphaModifier = static_cast<wp_alpha_modifier_v1*>(
      wl_registry_bind(registry, name, &wp_alpha_modifier_v1_interface, 1));
#endif
#if defined(CORAL_UI_WAYLAND_XDG_FOREIGN)
  } else if (std::strcmp(interface, zxdg_exporter_v2_interface.name) == 0) {
    platform->mExporter = static_cast<zxdg_exporter_v2*>(
      wl_registry_bind(registry, name, &zxdg_exporter_v2_interface, 1));
#endif
#if defined(CORAL_UI_WAYLAND_XDG_DECORATION)
  } else if (std::strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
    platform->mDecorationManager = static_cast<zxdg_decoration_manager_v1*>(
      wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
#endif
#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
  } else if (std::strcmp(interface, wp_cursor_shape_manager_v1_interface.name) == 0) {
    platform->mCursorShapeManager = static_cast<wp_cursor_shape_manager_v1*>(
      wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, 1));
#endif
#if defined(CORAL_UI_WAYLAND_FRACTIONAL_SCALE)
  } else if (std::strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0) {
    platform->mFractionalScaleManager = static_cast<wp_fractional_scale_manager_v1*>(
      wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, 1));
#endif
  } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
    auto* output = static_cast<wl_output*>(
      wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 3u)));
    platform->mOutputs.push_back(OutputEntry{ output, 1 });
    platform->mOutputListener.geometry = [](void*, wl_output*, int32_t, int32_t, int32_t,
        int32_t, int32_t, const char*, const char*, int32_t) {};
    platform->mOutputListener.mode = [](void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {};
    platform->mOutputListener.done = [](void*, wl_output*) {};
    platform->mOutputListener.scale = [](void* data, wl_output* output, int32_t factor) {
      auto* platform = static_cast<WaylandPlatform*>(data);
      for (OutputEntry& entry: platform->mOutputs) {
        if (entry.output == output) entry.scale = std::max(1, factor);
      }
      platform->applyOutputScale();
    };
    platform->mOutputListener.name = [](void*, wl_output*, const char*) {};
    platform->mOutputListener.description = [](void*, wl_output*, const char*) {};
    wl_output_add_listener(output, &platform->mOutputListener, platform);
  }
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
  else if (std::strcmp(interface, zwp_text_input_manager_v1_interface.name) == 0) {
    platform->mTextInputManager = static_cast<zwp_text_input_manager_v1*>(
      wl_registry_bind(registry, name, &zwp_text_input_manager_v1_interface, 1));
  }
#endif
#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
  else if (std::strcmp(interface, zwp_text_input_manager_v3_interface.name) == 0) {
    platform->mTextInputManagerV3 = static_cast<zwp_text_input_manager_v3*>(
      wl_registry_bind(registry, name, &zwp_text_input_manager_v3_interface, 1));
  }
#endif
}

void WaylandPlatform::seatCapabilities(void* data, wl_seat* seat, uint32_t capabilities) {
  auto* platform = static_cast<WaylandPlatform*>(data);
  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && platform->mPointer == nullptr) {
    platform->mPointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(platform->mPointer, &kPointerListener, platform);
#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
    if (platform->mCursorShapeManager != nullptr) {
      platform->mCursorShapeDevice = wp_cursor_shape_manager_v1_get_pointer(
        platform->mCursorShapeManager, platform->mPointer);
    }
#endif
  }
  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && platform->mKeyboard == nullptr) {
    platform->mKeyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(platform->mKeyboard, &kKeyboardListener, platform);
  }
}

const wl_seat_listener WaylandPlatform::kSeatListener = {
  WaylandPlatform::seatCapabilities,
  [](void*, wl_seat*, const char*) {},
};

const wl_data_offer_listener WaylandPlatform::kDropOfferListener = {
  [](void* data, wl_data_offer*, const char* mime) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (mime != nullptr && (std::strcmp(mime, "text/uri-list") == 0 ||
        std::strcmp(mime, "text/plain") == 0)) {
      platform->mDropMime = mime;
    }
  },
  [](void*, wl_data_offer*, uint32_t) {},
  [](void*, wl_data_offer*, uint32_t) {},
};

const wl_data_device_listener WaylandPlatform::kDataDeviceListener = {
  [](void* data, wl_data_device*, wl_data_offer* offer) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (offer != nullptr) wl_data_offer_add_listener(offer, &kDropOfferListener, platform);
  },
  [](void* data, wl_data_device*, uint32_t, wl_surface*, wl_fixed_t x, wl_fixed_t y,
      wl_data_offer* offer) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mDropOffer != nullptr) {
      wl_data_offer_destroy(platform->mDropOffer);
    }
    platform->mDropOffer = offer;
    platform->mDropMime.clear();
    platform->mDropPosition = { static_cast<float>(wl_fixed_to_double(x)) * platform->mScale,
      static_cast<float>(wl_fixed_to_double(y)) * platform->mScale };
  },
  [](void* data, wl_data_device*) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    platform->clearDropOffer();
  },
  [](void* data, wl_data_device*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    platform->mDropPosition = { static_cast<float>(wl_fixed_to_double(x)) * platform->mScale,
      static_cast<float>(wl_fixed_to_double(y)) * platform->mScale };
  },
  [](void* data, wl_data_device*) {
    static_cast<WaylandPlatform*>(data)->readDrop();
  },
  [](void*, wl_data_device*, wl_data_offer*) {},
};

const wl_keyboard_listener WaylandPlatform::kKeyboardListener = {
  [](void* data, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || fd < 0 || size == 0) {
      if (fd >= 0) close(fd);
      return;
    }
    void* map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (map == MAP_FAILED) return;
    if (platform->mXkbContext == nullptr) {
      platform->mXkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    }
    if (platform->mXkbContext != nullptr) {
      if (platform->mXkbKeymap != nullptr) xkb_keymap_unref(platform->mXkbKeymap);
      platform->mXkbKeymap = xkb_keymap_new_from_string(platform->mXkbContext,
        static_cast<const char*>(map), XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
      if (platform->mXkbKeymap != nullptr) {
        if (platform->mXkbState != nullptr) xkb_state_unref(platform->mXkbState);
        platform->mXkbState = xkb_state_new(platform->mXkbKeymap);
      }
    }
    munmap(map, size);
  },
  [](void* data, wl_keyboard*, uint32_t serial, wl_surface*, wl_array*) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    platform->mKeyboardEnterSerial = serial;
    setWaylandClipboardSerial(serial);
    if (platform->mWantsTextInput) {
      platform->updateTextInput(platform->mHost != nullptr
        ? platform->mHost->focusedLayer() : nullptr);
    }
  },
  [](void*, wl_keyboard*, uint32_t, wl_surface*) {},
  [](void* data, wl_keyboard*, uint32_t serial, uint32_t, uint32_t key, uint32_t state) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mXkbState == nullptr || platform->mHost == nullptr) return;
    const xkb_keycode_t code = key + 8;
    const xkb_keysym_t symbol = xkb_state_key_get_one_sym(platform->mXkbState, code);
    const bool pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
    // The clipboard owner needs the serial of the input event that triggered a copy. Tracking it only
    // on keyboard enter made every shortcut copy after the first one use a stale serial.
    if (pressed) setWaylandClipboardSerial(serial);
    KeyEvent event;
    event.key = keyFromXkb(symbol);
    const auto modifier = [platform](const char* name) {
      return xkb_state_mod_name_is_active(platform->mXkbState, name, XKB_STATE_MODS_EFFECTIVE) > 0;
    };
    event.shift = modifier(XKB_MOD_NAME_SHIFT);
    event.control = modifier(XKB_MOD_NAME_CTRL);
    event.alt = modifier(XKB_MOD_NAME_ALT);
    event.meta = modifier(XKB_MOD_NAME_LOGO);
    platform->mHost->dispatchKey(event, pressed);
    // Ctrl/Alt/Meta combinations are commands, not text: inserting the character as well would turn
    // Ctrl+S into a literal "s" after the shortcut ran.
    if (pressed && !event.control && !event.alt && !event.meta) {
      const uint32_t character = xkb_state_key_get_utf32(platform->mXkbState, code);
      if (character >= 32 && character != 127 && character != 0xFFFFFFFFu) {
        platform->mHost->dispatchText(character);
      }
    }
  },
  [](void* data, wl_keyboard*, uint32_t, uint32_t depressed, uint32_t latched, uint32_t locked,
      uint32_t group) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mXkbState != nullptr) {
      xkb_state_update_mask(platform->mXkbState, depressed, latched, locked, 0, 0, group);
    }
  },
  [](void*, wl_keyboard*, int32_t, int32_t) {},
};

const wl_pointer_listener WaylandPlatform::kPointerListener = {
  [](void* data, wl_pointer*, uint32_t serial, wl_surface*, wl_fixed_t x, wl_fixed_t y) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    platform->mPointerEnterSerial = serial;
#if defined(CORAL_UI_WAYLAND_CURSOR_SHAPE)
    if (!platform->useThemeCursor()) {
      // `set_shape` requires the serial of the enter that made this pointer valid.
      platform->applyCursorShape(platform->mCursor);
    }
#endif
#if defined(CORAL_UI_WAYLAND_CURSOR_THEME)
    platform->applyThemeCursor();
#endif
    platform->mPointerPosition = { static_cast<float>(wl_fixed_to_double(x)) * platform->mScale,
      static_cast<float>(wl_fixed_to_double(y)) * platform->mScale };
    if (platform->mHost != nullptr) {
      PointerEvent event;
      event.position = platform->mPointerPosition;
      platform->mHost->dispatchPointer(event, 0);
    }
  },
  [](void* data, wl_pointer*, uint32_t, wl_surface*) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost != nullptr) {
      // Without this the last hovered control keeps its hover state after the pointer leaves.
      PointerEvent event;
      event.position = platform->mPointerPosition;
      event.inside = false;
      platform->mHost->dispatchPointer(event, 0);
    }
    platform->mPointerPosition = {};
  },
  [](void* data, wl_pointer*, uint32_t, wl_fixed_t x, wl_fixed_t y) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    platform->mPointerPosition = { static_cast<float>(wl_fixed_to_double(x)) * platform->mScale,
      static_cast<float>(wl_fixed_to_double(y)) * platform->mScale };
    if (platform->mHost != nullptr) {
      PointerEvent event;
      event.position = platform->mPointerPosition;
      platform->mHost->dispatchPointer(event, 0);
    }
  },
  [](void* data, wl_pointer*, uint32_t serial, uint32_t, uint32_t button, uint32_t state) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) platform->mPointerButtonSerial = serial;
    if (platform->mHost == nullptr) return;
    // Client-side decorations: the caption band and the resize border are not native pixels, so a
    // press there has to start the compositor's interactive move/resize with this button's serial.
    if (state == WL_POINTER_BUTTON_STATE_PRESSED && button == 0x110 && platform->mToplevel != nullptr) {
      const Point local{ platform->mPointerPosition.x / platform->mScale,
        platform->mPointerPosition.y / platform->mScale };
      const WindowHit hit = platform->mHost->dispatchHitTest(local);
      uint32_t edge = 0;
      switch (hit) {
        case WindowHit::Caption:
          xdg_toplevel_move(platform->mToplevel, platform->mSeat, serial);
          return;
        case WindowHit::ResizeTop: edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP; break;
        case WindowHit::ResizeBottom: edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM; break;
        case WindowHit::ResizeLeft: edge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT; break;
        case WindowHit::ResizeRight: edge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT; break;
        case WindowHit::ResizeTopLeft: edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT; break;
        case WindowHit::ResizeTopRight: edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT; break;
        case WindowHit::ResizeBottomLeft: edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT; break;
        case WindowHit::ResizeBottomRight: edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT; break;
        default: break;
      }
      if (edge != 0) {
        xdg_toplevel_resize(platform->mToplevel, platform->mSeat, serial, edge);
        return;
      }
    }
    PointerEvent event;
    event.position = platform->mPointerPosition;
    event.button = button == 0x110 ? PointerButton::Primary
      : (button == 0x111 ? PointerButton::Secondary : PointerButton::Middle);
    event.shift = platform->mXkbState != nullptr &&
      xkb_state_mod_name_is_active(platform->mXkbState, XKB_MOD_NAME_SHIFT,
        XKB_STATE_MODS_EFFECTIVE) > 0;
    platform->mHost->dispatchPointer(event, state == WL_POINTER_BUTTON_STATE_PRESSED ? 1 : 2);
  },
  [](void* data, wl_pointer*, uint32_t timeMs, uint32_t axis, wl_fixed_t value) {
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL && axis != WL_POINTER_AXIS_HORIZONTAL_SCROLL) return;
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost == nullptr) return;
    ScrollEvent event;
    event.position = platform->mPointerPosition;
    // Wayland reports 10 surface units per notch on the traditional axis event. Trackpads send small
    // fractional values, which is what makes smooth scrolling work without a separate code path.
    const float pixels = static_cast<float>(-wl_fixed_to_double(value) * 48.0);
    if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
      event.deltaX = pixels;
    } else {
      event.deltaY = pixels;
    }
    event.timestamp = std::chrono::steady_clock::time_point(
      std::chrono::milliseconds(static_cast<uint64_t>(timeMs)));
    platform->mHost->dispatchScroll(event);
  },
  [](void*, wl_pointer*) {},
  [](void*, wl_pointer*, uint32_t) {},
  [](void*, wl_pointer*, uint32_t, uint32_t) {},
  [](void*, wl_pointer*, uint32_t, wl_fixed_t) {},
};

void WaylandPlatform::wmPing(void*, xdg_wm_base* base, uint32_t serial) {
  xdg_wm_base_pong(base, serial);
}

const xdg_wm_base_listener WaylandPlatform::kWmListener = { WaylandPlatform::wmPing };

void WaylandPlatform::xdgConfigure(void* data, xdg_surface* surface, uint32_t serial) {
  xdg_surface_ack_configure(surface, serial);
  static_cast<WaylandPlatform*>(data)->mConfigured = true;
}

const xdg_surface_listener WaylandPlatform::kXdgSurfaceListener = { WaylandPlatform::xdgConfigure };

void WaylandPlatform::toplevelConfigure(void* data, xdg_toplevel*, int32_t width, int32_t height,
    wl_array* states) {
  auto* platform = static_cast<WaylandPlatform*>(data);
  // The compositor owns maximize/minimize state. Reading the array lets the window report what the
  // user sees instead of the value the last `Window::maximize()` optimistically stored.
  bool maximized = false;
  const bool minimized = false;
  bool fullscreen = false;
  bool activated = false;
  if (states != nullptr) {
    for (size_t offset = 0; offset + sizeof(uint32_t) <= states->size; offset += sizeof(uint32_t)) {
      uint32_t state = 0;
      std::memcpy(&state, static_cast<const char*>(states->data) + offset, sizeof(state));
      switch (state) {
        case XDG_TOPLEVEL_STATE_MAXIMIZED: maximized = true; break;
        case XDG_TOPLEVEL_STATE_FULLSCREEN: fullscreen = true; break;
        case XDG_TOPLEVEL_STATE_ACTIVATED: activated = true; break;
        default: break;
      }
    }
  }
  platform->mMaximized = maximized;
  platform->mMinimized = minimized;
  platform->mFullscreen = fullscreen;
  platform->mActivated = activated;
  if (platform->mHost != nullptr) {
    platform->mHost->platformStateChanged(maximized, minimized, fullscreen);
  }
  // A maximized configure legitimately carries 0x0 ("client decides"); keep the current size then.
  if (width <= 0 || height <= 0) return;
  platform->mLogicalWidth = width;
  platform->mLogicalHeight = height;
  platform->updateViewport();
  const int physicalWidth = std::max(1, static_cast<int>(std::lround(width * platform->mScale)));
  const int physicalHeight = std::max(1, static_cast<int>(std::lround(height * platform->mScale)));
  platform->mWidth = physicalWidth;
  platform->mHeight = physicalHeight;
  platform->resizeEglWindow();
  if (platform->mSurface != nullptr) {
    platform->mSurface->resize(static_cast<uint32_t>(physicalWidth),
      static_cast<uint32_t>(physicalHeight));
    if (platform->mHost != nullptr) platform->mHost->platformResized(physicalWidth, physicalHeight);
  }
  platform->mConfigured = true;
}

void WaylandPlatform::toplevelClose(void* data, xdg_toplevel*) {
  auto* platform = static_cast<WaylandPlatform*>(data);
  if (platform->mHost != nullptr) platform->mHost->requestClose();
}

const xdg_toplevel_listener WaylandPlatform::kToplevelListener = {
  WaylandPlatform::toplevelConfigure, WaylandPlatform::toplevelClose,
  WaylandPlatform::toplevelConfigureBounds, WaylandPlatform::toplevelWmCapabilities,
};

#if defined(CORAL_UI_WAYLAND_FRACTIONAL_SCALE)
void WaylandPlatform::fractionalPreferred(void* data, wp_fractional_scale_v1*, uint32_t scale) {
  auto* platform = static_cast<WaylandPlatform*>(data);
  platform->setScale(static_cast<float>(scale) / 120.0f, true);
}

const wp_fractional_scale_v1_listener WaylandPlatform::kFractionalScaleListener = {
  WaylandPlatform::fractionalPreferred,
};
#endif

#if defined(CORAL_UI_WAYLAND_TEXT_INPUT)
const zwp_text_input_v1_listener WaylandPlatform::kTextInputListener = {
  [](void* data, zwp_text_input_v1*, wl_surface*) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    platform->updateTextInput(platform->mHost != nullptr ? platform->mHost->focusedLayer() : nullptr);
  },
  [](void* data, zwp_text_input_v1*) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost != nullptr) platform->mHost->dispatchComposition(std::string());
    platform->mTextInputActive = false;
  },
  [](void*, zwp_text_input_v1*, wl_array*) {},
  [](void*, zwp_text_input_v1*, uint32_t) {},
  [](void* data, zwp_text_input_v1*, uint32_t, const char* text, const char* commit) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost == nullptr) return;
    if (commit != nullptr && *commit != '\0') platform->mHost->dispatchCommit(commit);
    platform->mHost->dispatchComposition(text != nullptr ? text : "");
  },
  [](void*, zwp_text_input_v1*, uint32_t, uint32_t, uint32_t) {},
  [](void*, zwp_text_input_v1*, int32_t) {},
  [](void* data, zwp_text_input_v1*, uint32_t, const char* text) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost != nullptr) {
      platform->mHost->dispatchCommit(text != nullptr ? text : "");
    }
  },
  [](void*, zwp_text_input_v1*, int32_t, int32_t) {},
  [](void* data, zwp_text_input_v1*, int32_t index, uint32_t length) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost != nullptr) platform->mHost->dispatchDelete(index, length);
  },
  [](void*, zwp_text_input_v1*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {},
  [](void*, zwp_text_input_v1*, uint32_t, const char*) {},
  [](void*, zwp_text_input_v1*, uint32_t, uint32_t) {},
};
#endif

#if defined(CORAL_UI_WAYLAND_TEXT_INPUT_V3)
const zwp_text_input_v3_listener WaylandPlatform::kTextInputListenerV3 = {
  [](void* data, zwp_text_input_v3*, wl_surface*) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    platform->updateTextInput(platform->mHost != nullptr ? platform->mHost->focusedLayer() : nullptr);
  },
  [](void* data, zwp_text_input_v3*, wl_surface*) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost != nullptr) platform->mHost->dispatchComposition(std::string());
    platform->mTextInputActive = false;
  },
  [](void* data, zwp_text_input_v3*, const char* text, int32_t, int32_t) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost != nullptr) {
      platform->mHost->dispatchComposition(text != nullptr ? text : "");
    }
  },
  [](void* data, zwp_text_input_v3*, const char* text) {
    auto* platform = static_cast<WaylandPlatform*>(data);
    if (platform->mHost != nullptr) {
      platform->mHost->dispatchCommit(text != nullptr ? text : "");
    }
  },
  [](void* data, zwp_text_input_v3*, uint32_t beforeLength, uint32_t afterLength) {
    static_cast<WaylandPlatform*>(data)->dispatchDeleteCharacters(
      static_cast<int32_t>(beforeLength), static_cast<int32_t>(afterLength));
  },
  [](void*, zwp_text_input_v3*, uint32_t) {},
};
#endif

std::unique_ptr<WindowPlatform> createWaylandPlatform(const Window::Options& options,
    backend::Device* sharedDevice, std::string* diagnostics) {
  auto platform = std::make_unique<WaylandPlatform>(options, sharedDevice);
  if (!platform->initialize(diagnostics)) return nullptr;
  return platform;
}

} // namespace coral::ui::detail




