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

#include "WindowPlatform.h"

#include <graphics/Bitmap.h>
#include <graphics/Canvas.h>

#include <utility>

namespace coral::ui::detail {

namespace {

class RasterPlatform final : public WindowPlatform {
 public:
  RasterPlatform(const int width, const int height) : mWidth(width), mHeight(height) {}

  bool initialize(std::string* diagnostics) {
    if (!allocate(diagnostics)) return false;
    return true;
  }

  [[nodiscard]] Window::NativePlatform kind() const override {
    return Window::NativePlatform::Auto;
  }
  [[nodiscard]] bool headless() const override {
    return true;
  }
  [[nodiscard]] int width() const override {
    return mWidth;
  }
  [[nodiscard]] int height() const override {
    return mHeight;
  }
  [[nodiscard]] float dpiScale() const override {
    return 1.0f;
  }

  bool pump(WindowHost& host) override {
    (void)host;
    return true;
  }
  [[nodiscard]] graphics::Canvas* beginFrame() override {
    mCanvas = graphics::Canvas::create(mBitmap);
    return mCanvas.get();
  }
  bool present() override {
    return mCanvas != nullptr;
  }
  [[nodiscard]] bool supportsRetainedFrames() const override {
    return false;
  }
  [[nodiscard]] bool snapshot(graphics::Bitmap* out) override {
    if (out == nullptr || mBitmap.isEmpty()) return false;
    *out = mBitmap;
    return true;
  }
  bool resize(uint32_t width, uint32_t height) override {
    if (width == 0 || height == 0) return false;
    mWidth = static_cast<int>(width);
    mHeight = static_cast<int>(height);
    return allocate(nullptr);
  }
  bool recreate() override {
    return allocate(nullptr);
  }

  [[nodiscard]] std::string diagnostics() const override {
    return mDiagnostics;
  }
  [[nodiscard]] graphics::gpu::NativeDisplay nativeDisplay() const override {
    return {};
  }

 private:
  bool allocate(std::string* diagnostics) {
    mBitmap = graphics::Bitmap::allocate(mWidth, mHeight);
    if (mBitmap.isEmpty()) {
      mDiagnostics = "the raster frame buffer could not be allocated";
      if (diagnostics != nullptr) *diagnostics = mDiagnostics;
      return false;
    }
    mCanvas.reset();
    mDiagnostics.clear();
    return true;
  }

  int mWidth = 0;
  int mHeight = 0;
  graphics::Bitmap mBitmap;
  std::unique_ptr<graphics::Canvas> mCanvas;
  std::string mDiagnostics;
};

} // namespace anonymous

std::unique_ptr<WindowPlatform> createRasterPlatform(const Window::Options& options,
    backend::Device* sharedDevice, std::string* diagnostics) {
  (void)sharedDevice;
  auto platform = std::make_unique<RasterPlatform>(options.width, options.height);
  if (!platform->initialize(diagnostics)) return nullptr;
  return platform;
}

} // namespace coral::ui::detail
