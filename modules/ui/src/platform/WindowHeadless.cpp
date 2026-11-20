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

#include "WindowPlatform.h"

#include <gpu/Surface.h>

#include <utility>

namespace coral::ui::detail {

namespace {

class HeadlessPlatform final : public WindowPlatform {
 public:
  HeadlessPlatform(int width, int height, RendererApi api, backend::Device* sharedDevice,
      bool deferSurface)
      : mWidth(width), mHeight(height), mApi(api), mSharedDevice(sharedDevice),
        mDeferSurface(deferSurface) {}

  bool initialize(std::string* diagnostics) {
    if (mDeferSurface) {
      // The threaded render host owns the surface and creates it on its own thread; this platform
      // only provides the logical window (size/DPI/event pump) on the UI thread.
      return true;
    }
    graphics::gpu::SurfaceConfiguration configuration;
    configuration.width = static_cast<uint32_t>(mWidth);
    configuration.height = static_cast<uint32_t>(mHeight);
    configuration.preferred = toBackend(mApi);
    configuration.stencil = true;
    configuration.label = "ui-headless";
    if (mSharedDevice != nullptr) {
      mSurface = graphics::gpu::Surface::makeOn(mSharedDevice, configuration);
    } else {
      mSurface = graphics::gpu::Surface::make(configuration);
      if (mSurface != nullptr) {
        // Keep the device alive across resizes: a new `Surface` made without one would create a second
        // device, and the window would still hold the first device's context.
        mDeviceOwner.reset(mSurface->releaseDevice());
        if (mDeviceOwner != nullptr) {
          mSurface = graphics::gpu::Surface::makeOn(mDeviceOwner.get(), configuration);
        }
      }
    }
    if (mSurface == nullptr) {
      if (diagnostics != nullptr) *diagnostics = "the headless GPU surface could not be created";
      return false;
    }
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
    return mSurface != nullptr ? mSurface->beginFrame() : nullptr;
  }
  /** The offscreen surface reports its render time directly; the window surfaces go through `FrameStages`. */
  [[nodiscard]] FrameRenderTiming lastFrameRenderTiming() const override {
    if (mSurface == nullptr) return {};
    const double render = mSurface->lastFrameRenderMilliseconds();
    return FrameRenderTiming{ render, 0.0, mSurface->lastFrameGpuMilliseconds(), render,
      mSurface->residentBytes() };
  }

  [[nodiscard]] bool outputIsHdr() const override { return false; }
  bool present() override {
    return mSurface != nullptr && mSurface->submit();
  }
  [[nodiscard]] bool supportsBoundarySubmission() const override {
    return mSurface != nullptr;
  }
  bool presentBoundaries(const std::vector<graphics::gpu::Surface::ListItem>& items) override {
    return mSurface != nullptr && mSurface->submitLists(items);
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
  [[nodiscard]] bool supportsRetainedFrames() const override { return true; }
  bool presentRetained() override {
    return mSurface != nullptr && mSurface->submitRetained();
  }
  [[nodiscard]] bool supportsDamageRects() const override { return true; }
  bool presentDamage(const Rect& damage) override {
    return mSurface != nullptr && mSurface->submitDamage(damage);
  }
  [[nodiscard]] bool snapshot(graphics::Bitmap* out) override {
    if (mSurface == nullptr || out == nullptr) return false;
    *out = mSurface->snapshot();
    return !out->isEmpty();
  }
  bool resize(uint32_t width, uint32_t height) override {
    if (width == 0 || height == 0) return false;
    if (mDeferSurface) {
      mWidth = static_cast<int>(width);
      mHeight = static_cast<int>(height);
      return true;
    }
    graphics::gpu::SurfaceConfiguration configuration;
    configuration.width = width;
    configuration.height = height;
    configuration.preferred = toBackend(mApi);
    configuration.stencil = true;
    configuration.label = "ui-headless";
    backend::Device* device = mSharedDevice != nullptr ? mSharedDevice : mDeviceOwner.get();
    auto replacement = device != nullptr
      ? graphics::gpu::Surface::makeOn(device, configuration)
      : graphics::gpu::Surface::make(configuration);
    if (replacement == nullptr) {
      mDiagnostics = "the headless GPU surface could not be resized";
      return false;
    }
    mSurface = std::move(replacement);
    mWidth = static_cast<int>(width);
    mHeight = static_cast<int>(height);
    return true;
  }
  bool recreate() override {
    return resize(static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight));
  }
  [[nodiscard]] std::string diagnostics() const override {
    return mDiagnostics;
  }
  [[nodiscard]] graphics::gpu::NativeDisplay nativeDisplay() const override {
    return {};
  }

  [[nodiscard]] backend::Device* device() const override {
    return mSurface != nullptr ? mSurface->device() : nullptr;
  }

  [[nodiscard]] backend::Device* releaseDevice() override {
    if (mDeviceOwner != nullptr) {
      backend::Device* device = mDeviceOwner.release();
      // The caller now owns the device; this platform keeps borrowing it so a later resize does not
      // create a second device behind the window's back.
      mSharedDevice = device;
      return device;
    }
    return mSurface != nullptr ? mSurface->releaseDevice() : nullptr;
  }

  [[nodiscard]] uint32_t sampleCount() const override {
    return mSurface != nullptr ? mSurface->sampleCount() : 1;
  }

 private:
  int mWidth = 0;
  int mHeight = 0;
  RendererApi mApi = RendererApi::Auto;
  backend::Device* mSharedDevice = nullptr;
  std::unique_ptr<backend::Device> mDeviceOwner;
  bool mDeferSurface = false;
  std::unique_ptr<graphics::gpu::Surface> mSurface;
  std::string mDiagnostics;
};

} // namespace

std::unique_ptr<WindowPlatform> createHeadlessPlatform(const Window::Options& options,
    backend::Device* sharedDevice, std::string* diagnostics) {
  auto platform = std::make_unique<HeadlessPlatform>(options.width, options.height,
    options.rendererApi, sharedDevice, options.renderHost == RenderHostKind::RenderThread);
  if (!platform->initialize(diagnostics)) return nullptr;
  return platform;
}

} // namespace coral::ui::detail

