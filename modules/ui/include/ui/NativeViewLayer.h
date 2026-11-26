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

#ifndef CORAL_UI_NATIVEVIEWLAYER_H
#define CORAL_UI_NATIVEVIEWLAYER_H

#include <ui/Layer.h>

#include <chrono>

namespace coral::ui {

/**
 * Hosts a native platform view (for example an `NSView*` on macOS) above the Metal content at this
 * layer's absolute logical bounds. The native view is borrowed and must outlive the layer.
 *
 * This is the platform-neutral host for native-control extensions. The core layer only synchronizes
 * attachment, frame and visibility through `Window::setNativeViewBounds`/`removeNativeView`; each
 * `WindowPlatform` backend decides what a native view is. Concrete AppKit controls live under
 * `ui/macos` and `coral::ui::macos`, not in the cross-platform control namespace. On a platform
 * whose backend has not implemented the hooks, the layer is inert.
 */
class NativeViewLayer : public Layer {
 public:
  explicit NativeViewLayer(void* nativeView = nullptr);
  ~NativeViewLayer() override;

  void setNativeView(void* nativeView);
  [[nodiscard]] void* nativeView() const noexcept {
    return mNativeView;
  }
  /** Preferred logical size used by measure(); zero axes fall back to the parent's constraint. */
  void setPreferredSize(Size size);
  [[nodiscard]] Size preferredSize() const noexcept {
    return mPreferredSize;
  }
  Size measure(const Size& available) override;
  [[nodiscard]] Size intrinsicSize() const override {
    return mPreferredSize;
  }

  void layout() override;
  void onBoundsSizeChanged() override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  void drawSelf(RenderContext& context) override;
  void onWindowChanged(Window* previous, Window* current) override;
  void onVisibilityChanged(bool visible) override;

 private:
  void syncFrame();

  void* mNativeView = nullptr;
  Size mPreferredSize{};
};

} // namespace coral::ui

#endif // CORAL_UI_NATIVEVIEWLAYER_H
