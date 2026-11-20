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

#include <ui/Canvas.h>
#include <ui/CanvasBackend.h>

#include <graphics/Canvas.h>

#include <utility>

namespace coral::ui {

struct Canvas::Impl {
  explicit Impl(graphics::Canvas& source) : canvas(&source) {}
  graphics::Canvas* canvas = nullptr;
};

Canvas::Canvas() = default;
Canvas::~Canvas() = default;
Canvas::Canvas(const Canvas&) = default;
Canvas& Canvas::operator=(const Canvas&) = default;
Canvas::Canvas(Canvas&&) noexcept = default;
Canvas& Canvas::operator=(Canvas&&) noexcept = default;
Canvas::Canvas(std::shared_ptr<Impl> impl) : mImpl(std::move(impl)) {}

namespace detail {

struct CanvasAccess {
  static Canvas make(graphics::Canvas& canvas) {
    return Canvas(std::make_shared<Canvas::Impl>(canvas));
  }
  static graphics::Canvas& native(const Canvas& canvas) {
    return *canvas.mImpl->canvas;
  }
};

Canvas makeCanvas(graphics::Canvas& canvas) {
  return CanvasAccess::make(canvas);
}

graphics::Canvas& nativeCanvas(Canvas& canvas) {
  return CanvasAccess::native(canvas);
}

} // namespace detail

bool Canvas::isRecording() const noexcept {
  return mImpl != nullptr && mImpl->canvas->isRecording();
}

bool Canvas::inRecordingScope() const {
  return mImpl != nullptr && mImpl->canvas->inRecordingScope();
}

bool Canvas::hasClipPath() const noexcept {
  return mImpl != nullptr && mImpl->canvas->hasClipPath();
}

Matrix Canvas::transform() const noexcept {
  return mImpl != nullptr ? mImpl->canvas->transform() : Matrix{};
}

float Canvas::transformScale() const noexcept {
  return mImpl != nullptr ? mImpl->canvas->transformScale() : 1.0f;
}

Rect Canvas::deviceClipBounds() const noexcept {
  return mImpl != nullptr ? mImpl->canvas->deviceClipBounds() : Rect{};
}

void Canvas::save() {
  if (mImpl != nullptr) mImpl->canvas->save();
}

void Canvas::restore() {
  if (mImpl != nullptr) mImpl->canvas->restore();
}

void Canvas::clipRect(const Rect& rect) {
  if (mImpl != nullptr) mImpl->canvas->clipRect(rect);
}

void Canvas::clipPath(const Path& path) {
  if (mImpl != nullptr) mImpl->canvas->clipPath(path);
}

void Canvas::clear(const Color& color) {
  if (mImpl != nullptr) mImpl->canvas->clear(color);
}

void Canvas::translate(const float dx, const float dy) {
  if (mImpl != nullptr) mImpl->canvas->translate(dx, dy);
}

void Canvas::rotate(const float degrees) {
  if (mImpl != nullptr) mImpl->canvas->rotate(degrees);
}

void Canvas::scale(const float scale) {
  if (mImpl != nullptr) mImpl->canvas->scale(scale);
}

void Canvas::scale(const float scaleX, const float scaleY) {
  if (mImpl != nullptr) mImpl->canvas->scale(scaleX, scaleY);
}

void Canvas::concat(const Matrix& matrix) {
  if (mImpl != nullptr) mImpl->canvas->concat(matrix);
}

void Canvas::saveLayer(const Rect& bounds, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->saveLayer(bounds, paint);
}

void Canvas::saveLayerAlpha(const Rect& bounds, const uint8_t alpha) {
  if (mImpl != nullptr) mImpl->canvas->saveLayerAlpha(bounds, alpha);
}

void Canvas::maskLayer(const Bitmap& mask, const MaskMode mode, const bool alignToLayer) {
  if (mImpl != nullptr) mImpl->canvas->maskLayer(mask, mode, alignToLayer);
}

void Canvas::drawRect(const Rect& rect, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawRect(rect, paint);
}

void Canvas::drawRectStroke(const Rect& rect, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawRectStroke(rect, paint);
}

void Canvas::drawPath(const Path& path, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawPath(path, paint);
}

void Canvas::drawLine(const Point from, const Point to, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawLine(from, to, paint);
}

void Canvas::drawCircle(const float centreX, const float centreY, const float radius,
    const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawCircle(centreX, centreY, radius, paint);
}

void Canvas::drawRoundRect(const RoundRect& rect, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawRoundRect(rect, paint);
}

void Canvas::drawText(const std::string& text, const float x, const float y, const Font& font,
    const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawText(text, x, y, font, paint);
}

void Canvas::drawBitmapRect(const Bitmap& image, const Rect& source, const Rect& destination,
    const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawBitmapRect(image, source, destination, paint);
}

void Canvas::drawBitmapRect(const Bitmap& image, const Rect& source, const Rect& destination,
    const SamplingOptions& sampling, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawBitmapRect(image, source, destination, sampling, paint);
}

void Canvas::drawImage(const Image& image, const float x, const float y, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawImage(image, x, y, paint);
}

void Canvas::drawImage(const Image& image, const float x, const float y,
    const SamplingOptions& sampling, const Paint& paint) {
  if (mImpl != nullptr) mImpl->canvas->drawImage(image, x, y, sampling, paint);
}

} // namespace coral::ui
