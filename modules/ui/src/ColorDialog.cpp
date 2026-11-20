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

#include <ui/ColorDialog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#endif

namespace coral::ui {

namespace {

#if defined(_WIN32)
COLORREF toColorRef(const Color color) {
  const auto channel = [](const float value) {
    return static_cast<BYTE>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
  };
  return RGB(channel(color.red), channel(color.green), channel(color.blue));
}

Color fromColorRef(const COLORREF value, const float alpha) {
  return Color::MakeRGBA8(GetRValue(value), GetGValue(value), GetBValue(value),
    static_cast<uint8_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
}
#endif

} // namespace

ColorDialogResult showColorDialog(const Color initial, const ColorDialogOptions& options) {
  ColorDialogResult result;
  result.color = initial;
#if defined(_WIN32)
  static COLORREF customColors[16] = {};
  CHOOSECOLORW chooser{};
  chooser.lStructSize = sizeof(CHOOSECOLORW);
  chooser.hwndOwner = GetActiveWindow();
  chooser.rgbResult = toColorRef(initial);
  chooser.lpCustColors = customColors;
  chooser.Flags = CC_FULLOPEN | CC_RGBINIT;
  if (options.title.empty()) {
    chooser.Flags &= ~CC_FULLOPEN;
  } else {
    // The common dialog takes its title from the resource; the public option is kept for the future
    // custom-template path and for platform backends that need it.
  }
  if (!ChooseColorW(&chooser)) return result;
  result.accepted = true;
  result.color = fromColorRef(chooser.rgbResult,
    options.preserveAlpha ? initial.alpha : 1.0f);
  return result;
#else
  (void)options;
  return result;
#endif
}

bool pickScreenColor(Color& out, std::string* error) {
#if defined(_WIN32)
  POINT point{};
  if (!GetCursorPos(&point)) {
    if (error != nullptr) *error = "GetCursorPos failed";
    return false;
  }
  HDC screen = GetDC(nullptr);
  if (screen == nullptr) {
    if (error != nullptr) *error = "GetDC failed";
    return false;
  }
  // Wait for a fresh press instead of sampling the click that opened the picker.
  bool wasDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  for (;;) {
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
      ReleaseDC(nullptr, screen);
      if (error != nullptr) *error = "cancelled";
      return false;
    }
    const bool down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (GetCursorPos(&point) && down && !wasDown) {
      const COLORREF pixel = GetPixel(screen, point.x, point.y);
      ReleaseDC(nullptr, screen);
      if (pixel == CLR_INVALID) {
        if (error != nullptr) *error = "GetPixel failed";
        return false;
      }
      out = Color::MakeRGBA8(GetRValue(pixel), GetGValue(pixel), GetBValue(pixel), 255);
      return true;
    }
    wasDown = down;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
#else
  (void)out;
  if (error != nullptr) *error = "the screen eyedropper is not implemented on this platform";
  return false;
#endif
}

} // namespace coral::ui
