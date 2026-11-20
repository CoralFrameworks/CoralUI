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

#ifndef CORAL_UI_RASTERRENDERER_H
#define CORAL_UI_RASTERRENDERER_H

#include <ui/Renderer.h>

#include <memory>

namespace coral::ui {

class Layer;
class TextRenderCache;

/**
 * A pure software `Renderer` for tests and tools.
 *
 * It owns a CPU bitmap and records the layer tree into a raster `Canvas`; no device, surface or
 * swapchain is created. `snapshot` returns the last frame without a readback.
 */
class RasterRenderer final : public Renderer {
 public:
  RasterRenderer(int width, int height);
  ~RasterRenderer() override;

  RasterRenderer(const RasterRenderer&) = delete;
  RasterRenderer& operator=(const RasterRenderer&) = delete;

  void setRoot(std::shared_ptr<Layer> root);
  [[nodiscard]] const std::shared_ptr<Layer>& root() const noexcept { return mRoot; }

  bool resize(int width, int height);

  [[nodiscard]] RendererKind rendererKind() const noexcept override;
  [[nodiscard]] const RendererCapabilities& capabilities() const noexcept override;
  bool renderFrame() override;
  bool recover() override;
  [[nodiscard]] bool snapshot(Bitmap* out) override;

  [[nodiscard]] int width() const noexcept { return mWidth; }
  [[nodiscard]] int height() const noexcept { return mHeight; }
  [[nodiscard]] const Bitmap& bitmap() const noexcept { return mBitmap; }
  [[nodiscard]] bool hasFrame() const noexcept { return mHasFrame; }

 private:
  bool allocateBitmap();

  int mWidth = 0;
  int mHeight = 0;
  Bitmap mBitmap;
  std::shared_ptr<Layer> mRoot;
  std::shared_ptr<TextRenderCache> mTextCache;
  RendererCapabilities mCapabilities;
  bool mHasFrame = false;
};

} // namespace coral::ui

#endif // CORAL_UI_RASTERRENDERER_H
