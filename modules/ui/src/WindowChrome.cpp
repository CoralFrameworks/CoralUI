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

#include <ui/WindowChrome.h>

#include <algorithm>

namespace coral::ui {

WindowHit hitTestWindowChrome(const Size size, const Point point,
    const WindowChromeMetrics& metrics) {
  if (point.x < 0.0f || point.y < 0.0f || point.x >= size.width || point.y >= size.height) {
    return WindowHit::None;
  }

  const float border = std::max(0.0f, metrics.resizeBorder);
  if (metrics.resizable && border > 0.0f) {
    const bool left = point.x < border;
    const bool right = point.x >= size.width - border;
    const bool top = point.y < border;
    const bool bottom = point.y >= size.height - border;
    // Corners win over edges so a diagonal drag is possible in the very corner pixel.
    if (top && left) return WindowHit::ResizeTopLeft;
    if (top && right) return WindowHit::ResizeTopRight;
    if (bottom && left) return WindowHit::ResizeBottomLeft;
    if (bottom && right) return WindowHit::ResizeBottomRight;
    if (left) return WindowHit::ResizeLeft;
    if (right) return WindowHit::ResizeRight;
    if (top) return WindowHit::ResizeTop;
    if (bottom) return WindowHit::ResizeBottom;
  }

  const float titleBar = std::max(0.0f, metrics.titleBarHeight);
  if (point.y < titleBar) {
    const float buttonWidth = std::max(0.0f, metrics.buttonWidth);
    if (buttonWidth > 0.0f) {
      float right = size.width;
      if (metrics.closable) {
        if (point.x >= right - buttonWidth) return WindowHit::Close;
        right -= buttonWidth;
      }
      if (metrics.maximizable) {
        if (point.x >= right - buttonWidth) return WindowHit::Maximize;
        right -= buttonWidth;
      }
      if (metrics.minimizable) {
        if (point.x >= right - buttonWidth) return WindowHit::Minimize;
      }
    }
    return WindowHit::Caption;
  }

  return WindowHit::Client;
}

} // namespace coral::ui
