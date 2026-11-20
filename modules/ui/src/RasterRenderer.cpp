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

#include <ui/RasterRenderer.h>

#include <ui/Layer.h>
#include <ui/RenderContext.h>
#include <ui/CanvasBackend.h>

#include "TextRenderCache.h"

#include <graphics/Canvas.h>

#include <utility>

namespace coral::ui {

RasterRenderer::RasterRenderer(const int width, const int height)
    : mWidth(width), mHeight(height), mTextCache(std::make_shared<TextRenderCache>()),
      mCapabilities(RendererCapabilities::raster()) {
  allocateBitmap();
}

RasterRenderer::~RasterRenderer() = default;

void RasterRenderer::setRoot(std::shared_ptr<Layer> root) {
  mRoot = std::move(root);
  if (mRoot != nullptr && mWidth > 0 && mHeight > 0) {
    mRoot->setBounds(Rect::MakeWH(static_cast<float>(mWidth), static_cast<float>(mHeight)));
    mRoot->layout();
  }
}

bool RasterRenderer::resize(const int width, const int height) {
  if (width <= 0 || height <= 0) return false;
  mWidth = width;
  mHeight = height;
  mHasFrame = false;
  if (!allocateBitmap()) return false;
  if (mRoot != nullptr) {
    mRoot->setBounds(Rect::MakeWH(static_cast<float>(mWidth), static_cast<float>(mHeight)));
    mRoot->layout();
  }
  return true;
}

RendererKind RasterRenderer::rendererKind() const noexcept {
  return RendererKind::Raster;
}

const RendererCapabilities& RasterRenderer::capabilities() const noexcept {
  return mCapabilities;
}

bool RasterRenderer::allocateBitmap() {
  if (mWidth <= 0 || mHeight <= 0) {
    mBitmap = Bitmap{};
    return false;
  }
  mBitmap = Bitmap::allocate(mWidth, mHeight);
  return !mBitmap.isEmpty();
}

bool RasterRenderer::renderFrame() {
  if (mBitmap.isEmpty() && !allocateBitmap()) return false;
  auto canvas = graphics::Canvas::create(mBitmap);
  if (canvas == nullptr) return false;
  canvas->clear(Color::MakeRGBA8(0, 0, 0, 0));
  if (mRoot != nullptr) {
    if (mRoot->bounds().getWidth() != static_cast<float>(mWidth) ||
        mRoot->bounds().getHeight() != static_cast<float>(mHeight)) {
      mRoot->setBounds(Rect::MakeWH(static_cast<float>(mWidth), static_cast<float>(mHeight)));
      mRoot->layout();
    }
    Canvas uiCanvas = detail::makeCanvas(*canvas);
    RenderContext context(uiCanvas, 1.0f, mTextCache,
      Size{ static_cast<float>(mWidth), static_cast<float>(mHeight) }, nullptr);
    mRoot->draw(context);
  }
  mHasFrame = true;
  return true;
}

bool RasterRenderer::recover() {
  mHasFrame = false;
  return allocateBitmap();
}

bool RasterRenderer::snapshot(Bitmap* out) {
  if (out == nullptr) return false;
  if (!mHasFrame && !renderFrame()) return false;
  *out = mBitmap;
  return !out->isEmpty();
}

} // namespace coral::ui
