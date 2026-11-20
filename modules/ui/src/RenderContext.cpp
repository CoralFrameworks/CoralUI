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

#include <ui/RenderContext.h>
#include <ui/CanvasBackend.h>
#include <ui/ContentFrameBackend.h>

#include "TextRenderCache.h"

#include <utility>

namespace coral::ui {

RenderContext::RenderContext(Canvas& canvas, const float uiScale,
    std::shared_ptr<TextRenderCache> textCache, const Size deviceSize,
    std::shared_ptr<const RendererCapabilities> capabilities) noexcept
    : mCanvas(canvas), mUiScale(uiScale > 0.0f ? uiScale : 1.0f), mDeviceSize(deviceSize),
      mTextCache(std::move(textCache)), mCapabilities(std::move(capabilities)) {}

const RendererCapabilities& RenderContext::capabilities() const noexcept {
  static const RendererCapabilities raster = RendererCapabilities::raster();
  return mCapabilities != nullptr ? *mCapabilities : raster;
}

void RenderContext::drawText(const std::string& text, const float x, const float baseline,
    const graphics::Font& font, const graphics::Paint& paint) {
  if (text.empty()) return;
  if (mTextCache != nullptr) {
    mTextCache->draw(*this, text, x, baseline, font, paint);
    return;
  }
  // A hand-built context (a unit test, a custom renderer) still gets device-resolution text; it just
  // does not keep the shaped blob between calls.
  TextPainter painter;
  painter.draw(*this, text, x, baseline, font, paint);
}

bool RenderContext::supportsExternalImage() const noexcept {
  // The capability snapshot is the authority when there is one; a hand-built context falls back to
  // what its canvas can actually do.
  if (mCapabilities != nullptr) {
    return mCapabilities->externalImages.supported;
  }
  return mCanvas.isRecording();
}

void RenderContext::recordContentFrame(const ContentFrame& frame, ContentHost::FrameFence fence) const {
  if (mContentFrames == nullptr || mContentFences == nullptr) return;
  if (!frame.valid()) return;
  mContentFrames->push_back(frame);
  if (fence) mContentFences->push_back(std::move(fence));
}

bool RenderContext::drawContentFrame(const ContentFrame& frame, const Rect& bounds,
    const graphics::Paint& paint) {
  const graphics::ExternalImage& image = detail::imageOf(frame);
  if (!image.isValid() || !supportsExternalImage()) {
    return false;
  }
  // Recorded on the canvas, not on a surface: the command lands where this layer is drawing, so the
  // canvas's transform, clip, group-opacity layer and blend state all apply to the content.
  detail::nativeCanvas(mCanvas).drawExternalImage(image, bounds, paint);
  return true;
}

} // namespace coral::ui
