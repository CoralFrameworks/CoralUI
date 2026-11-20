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

// Native multi-monitor probe. It moves one real window to every attached monitor and checks that
// the Win32 per-monitor-v2 DPI reports agree with the window's logical scale. It returns 77 when
// this session cannot create a native window, so CI can skip it rather than report a false failure.

#include <ui/ColorLayer.h>
#include <ui/Window.h>

#include <graphics/Color.h>

#include <cstdio>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

struct MonitorProbe {
  RECT bounds{};
  bool primary = false;
  DWORD width = 0;
  DWORD height = 0;
  DWORD refresh = 0;
  std::wstring device;
};

#if defined(_WIN32)
BOOL CALLBACK collectMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM context) {
  auto* monitors = reinterpret_cast<std::vector<MonitorProbe>*>(context);
  MONITORINFOEXW info{};
  info.cbSize = sizeof(MONITORINFOEXW);
  if (!GetMonitorInfoW(monitor, reinterpret_cast<LPMONITORINFO>(&info))) return TRUE;
  MonitorProbe probe;
  probe.bounds = info.rcMonitor;
  probe.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
  probe.device = info.szDevice;
  DEVMODEW mode{};
  mode.dmSize = sizeof(DEVMODEW);
  if (EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
    probe.width = mode.dmPelsWidth;
    probe.height = mode.dmPelsHeight;
    probe.refresh = mode.dmDisplayFrequency;
  }
  monitors->push_back(std::move(probe));
  return TRUE;
}
#endif

} // namespace

int main() {
#if !defined(_WIN32)
  std::printf("SKIP: the multi-monitor probe is Windows-only\n");
  return 77;
#else
  std::vector<MonitorProbe> monitors;
  EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&monitors));
  if (monitors.empty()) {
    std::printf("SKIP: no monitors are attached\n");
    return 77;
  }

  coral::ui::Window::Options options;
  options.headless = false;
  options.width = 640;
  options.height = 400;
  options.clearColor = graphics::Color::MakeRGBA8(24, 26, 32, 255);
  auto window = coral::ui::Window::create(options);
  if (window == nullptr || window->nativeHandle() == nullptr) {
    std::printf("SKIP: no native window could be created in this session\n");
    return 77;
  }
  window->setRoot(std::make_shared<coral::ui::ColorLayer>(
    graphics::Color::MakeRGBA8(56, 132, 220, 255)));

  const HWND hwnd = static_cast<HWND>(window->nativeHandle());
  int distinctDpi = 0;
  UINT previousDpi = 0;
  for (size_t index = 0; index < monitors.size(); ++index) {
    const MonitorProbe& monitor = monitors[index];
    const int x = monitor.bounds.left + 40;
    const int y = monitor.bounds.top + 40;
    SetWindowPos(hwnd, HWND_TOP, x, y, 640, 400, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    window->tick(false);
    if (!window->renderFrame()) {
      std::fprintf(stderr, "FAIL: monitor %zu did not render: %s\n", index,
        window->diagnostics().c_str());
      return 1;
    }

    const UINT dpi = GetDpiForWindow(hwnd);
    const float expectedScale = dpi == 0 ? 1.0f : static_cast<float>(dpi) / 96.0f;
    if (previousDpi != 0 && dpi != previousDpi) ++distinctDpi;
    previousDpi = dpi;
    if (!(window->dpiScale() > 0.0f) ||
        std::abs(window->dpiScale() - expectedScale) > 0.02f) {
      std::fprintf(stderr,
        "FAIL: monitor %zu reports DPI %u (expected scale %.2f) but the window reports %.2f\n",
        index, dpi, static_cast<double>(expectedScale),
        static_cast<double>(window->dpiScale()));
      return 1;
    }
    if (window->width() <= 0 || window->height() <= 0) {
      std::fprintf(stderr, "FAIL: monitor %zu produced an empty client area\n", index);
      return 1;
    }
    std::printf("monitor %zu%s: %lux%lu@%luHz bounds=(%ld,%ld)-(%ld,%ld) dpi=%u uiScale=%.2f\n",
      index, monitor.primary ? " (primary)" : "", static_cast<unsigned long>(monitor.width),
      static_cast<unsigned long>(monitor.height), static_cast<unsigned long>(monitor.refresh),
      monitor.bounds.left, monitor.bounds.top, monitor.bounds.right, monitor.bounds.bottom, dpi,
      static_cast<double>(window->uiScale()));
  }

  std::printf("multi-monitor probe: %zu monitor(s), %d DPI transition(s)%s\n", monitors.size(),
    distinctDpi, distinctDpi == 0 ? " (all monitors report the same DPI)" : "");
  return 0;
#endif
}
