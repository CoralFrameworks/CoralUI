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

#include <ui/NativeViewLayer.h>


#include <algorithm>

#include <ui/Window.h>

#include <utility>

namespace coral::ui {

NativeViewLayer::NativeViewLayer(void* nativeView) : mNativeView(nativeView) {}

NativeViewLayer::~NativeViewLayer() {
  if (mNativeView != nullptr && window() != nullptr) {
    window()->removeNativeView(mNativeView);
  }
}

void NativeViewLayer::setNativeView(void* nativeView) {
  if (mNativeView == nativeView) return;
  if (mNativeView != nullptr && window() != nullptr) window()->removeNativeView(mNativeView);
  mNativeView = nativeView;
  syncFrame();
  setNeedsPaint();
}

void NativeViewLayer::syncFrame() {
  if (mNativeView == nullptr || window() == nullptr) return;
  // A newly added layer has an empty bounds until its parent lays it out. Attaching the NSView at
  // (0, 0) would make it flash across the window (and over the gallery sidebar). Keep it detached
  // until a real frame exists.
  if (bounds().getWidth() <= 0.0f || bounds().getHeight() <= 0.0f) {
    window()->setNativeViewBounds(mNativeView, {}, false);
    return;
  }
  // Native views live in the platform hierarchy and do not inherit the layer transform. Translate the
  // frame by the layer's own and its ancestors' compositing offsets so a native control inside a
  // translated page (the sectioned gallery's right panel) stays in that panel.
  Rect frame = bounds();
  Point offset = translation();
  for (Layer* ancestor = parent(); ancestor != nullptr; ancestor = ancestor->parent()) {
    const Point ancestorTranslation = ancestor->translation();
    offset.x += ancestorTranslation.x;
    offset.y += ancestorTranslation.y;
  }
  frame = Rect::MakeXYWH(frame.getLeft() + offset.x, frame.getTop() + offset.y,
    frame.getWidth(), frame.getHeight());
  window()->setNativeViewBounds(mNativeView, frame, visible());
}

void NativeViewLayer::onWindowChanged(Window* previous, Window* current) {
  if (previous != nullptr && current == nullptr && mNativeView != nullptr) {
    previous->removeNativeView(mNativeView);
  } else if (current != nullptr) {
    syncFrame();
  }
}

void NativeViewLayer::onVisibilityChanged(bool visible) {
  (void)visible;
  syncFrame();
}

Size NativeViewLayer::measure(const Size& available) {
  const float width = mPreferredSize.width > 0.0f ? mPreferredSize.width : available.width;
  const float height = mPreferredSize.height > 0.0f ? mPreferredSize.height : available.height;
  setMeasuredSize({ std::max(0.0f, width), std::max(0.0f, height) });
  return measuredSize();
}

void NativeViewLayer::setPreferredSize(const Size size) {
  mPreferredSize = size;
  setNeedsLayout();
}

void NativeViewLayer::layout() {
  Layer::layout();
  syncFrame();
}

void NativeViewLayer::onBoundsSizeChanged() {
  // Parent layout positions hosted controls with setBounds, not always with layout(); sync as soon as
  // the first real size arrives so the native HWND/NSView is shown in the first presented frame.
  Layer::onBoundsSizeChanged();
  syncFrame();
}

void NativeViewLayer::onFrame(const std::chrono::steady_clock::time_point now) {
  Layer::onFrame(now);
  // A parent can move or resize this layer without a size change; keep the platform view in step even
  // when the page itself does not repaint.
  syncFrame();
}

void NativeViewLayer::drawSelf(RenderContext&) {
  syncFrame();
}

} // namespace coral::ui
