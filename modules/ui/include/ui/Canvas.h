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

#ifndef CORAL_UI_CANVAS_H
#define CORAL_UI_CANVAS_H

#include <ui/Types.h>
#include <ui/Geometry.h>

#include <cstdint>
#include <memory>
#include <string>

namespace coral::ui {

namespace detail {
struct CanvasAccess;
}

/**
 * The drawing service a UI layer sees.
 *
 * This is a pimpl façade over the graphics canvas: controls can draw rectangles, text, paths,
 * images and layers without including any graphics canvas header. The native canvas is reachable
 * only through the host-only `ui/CanvasBackend.h` bridge.
 */
class Canvas {
 public:
  Canvas();
  ~Canvas();
  Canvas(const Canvas&);
  Canvas& operator=(const Canvas&);
  Canvas(Canvas&&) noexcept;
  Canvas& operator=(Canvas&&) noexcept;

  [[nodiscard]] bool isRecording() const noexcept;
  [[nodiscard]] bool inRecordingScope() const;
  [[nodiscard]] bool hasClipPath() const noexcept;
  [[nodiscard]] Matrix transform() const noexcept;
  [[nodiscard]] float transformScale() const noexcept;
  [[nodiscard]] Rect deviceClipBounds() const noexcept;

  void save();
  void restore();
  void clipRect(const Rect& rect);
  void clipPath(const Path& path);
  void clear(const Color& color);
  void translate(float dx, float dy);
  void rotate(float degrees);
  void scale(float scale);
  void scale(float scaleX, float scaleY);
  void concat(const Matrix& matrix);

  void saveLayer(const Rect& bounds, const Paint& paint);
  void saveLayerAlpha(const Rect& bounds, uint8_t alpha);
  void maskLayer(const Bitmap& mask, MaskMode mode = MaskMode::kAlpha, bool alignToLayer = false);

  void drawRect(const Rect& rect, const Paint& paint);
  void drawRectStroke(const Rect& rect, const Paint& paint);
  void drawPath(const Path& path, const Paint& paint);
  void drawLine(Point from, Point to, const Paint& paint);
  void drawCircle(float centreX, float centreY, float radius, const Paint& paint);
  void drawRoundRect(const RoundRect& rect, const Paint& paint);
  void drawText(const std::string& text, float x, float y, const Font& font, const Paint& paint);
  void drawBitmapRect(const Bitmap& image, const Rect& source, const Rect& destination,
    const Paint& paint);
  void drawBitmapRect(const Bitmap& image, const Rect& source, const Rect& destination,
    const SamplingOptions& sampling, const Paint& paint);
  void drawImage(const Image& image, float x, float y, const Paint& paint);
  void drawImage(const Image& image, float x, float y, const SamplingOptions& sampling,
    const Paint& paint);

 private:
  friend struct detail::CanvasAccess;

  struct Impl;
  explicit Canvas(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> mImpl;
};

} // namespace coral::ui

#endif // CORAL_UI_CANVAS_H
