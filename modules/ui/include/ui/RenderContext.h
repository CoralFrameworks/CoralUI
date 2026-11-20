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

#ifndef CORAL_UI_RENDERCONTEXT_H
#define CORAL_UI_RENDERCONTEXT_H

#include <ui/ContentFrame.h>
#include <ui/ContentHost.h>
#include <ui/Canvas.h>
#include <ui/Types.h>

#include <ui/Geometry.h>
#include <ui/RendererCapabilities.h>

#include <memory>
#include <string>
#include <vector>

namespace graphics::gpu {
class DrawList;
} // namespace graphics::gpu

namespace coral::ui {

class TextRenderCache;

/**
 * What a layer draws into.
 *
 * Most layers only need the canvas. A content layer also needs the externally-produced texture route,
 * which is a recording on the canvas itself: the content-frame draw appends the command into the
 * frame's own draw list, so the current transform, clip, group opacity and blend mode all apply. That is
 * why this context no longer has to know whether the frame is a window or an offscreen surface: the
 * canvas is the recording either way, and capability is a property of the canvas.
 *
 * ## Drawing text
 *
 * Use `drawText`, not `canvas().drawText`, for UI text. The former rasterises at the device scale,
 * snaps to the physical pixel grid and reuses a shaped `TextBlob` through the window's text cache. A
 * direct `Canvas::drawText` is a low-level graphics call and cannot be intercepted for UI policy.
 */
class Layer;

class RenderContext {
public:
  RenderContext(Canvas& canvas, float uiScale = 1.0f,
    std::shared_ptr<TextRenderCache> textCache = nullptr, Size deviceSize = {},
    std::shared_ptr<const RendererCapabilities> capabilities = nullptr) noexcept;

  /**
   * One item of a plan-assembled frame: a layer whose recorded lists the frame will submit itself.
   *
   * A repaint boundary is collected with no translation and no clip. A scrolled viewport adds its content
   * with the scroll offset as the translation and the viewport as the clip, which is what lets the same
   * content list be reused while the viewport scrolls instead of being re-recorded.
   */
  struct PlannedItem {
    Layer* layer = nullptr;
    /**
     * A list the container recorded itself, used instead of borrowing the layer's.
     *
     * A viewport's scrollbar is its own paint and sits *above* the content it lends, but everything a
     * container draws into the frame's canvas is submitted before the lent lists -- so that paint has to
     * travel as a list of its own, after the content.
     */
    std::shared_ptr<const graphics::gpu::DrawList> list;
    /** Device-pixel translation applied when the item's lists are submitted. */
    Point translation{};
    /** Device-pixel clip; empty means the whole target. */
    Rect clip{};
  };

  /**
   * Collects the items a plan-mode frame does not draw itself, in paint order.
   *
   * With a sink installed the recorder draws everything *except* the collected items, so the frame
   * becomes "content list + boundary lists" rather than one flattened list. Per-layer recordings are
   * disabled while it is set (except on a boundary, which records the very list the frame lends).
   */
  void setPlanSink(std::vector<PlannedItem>* items) noexcept {
    mPlanItems = items;
  }
  [[nodiscard]] std::vector<PlannedItem>* planSink() const noexcept {
    return mPlanItems;
  }
  [[nodiscard]] bool assemblingPlan() const noexcept {
    return mPlanItems != nullptr;
  }

  [[nodiscard]] Canvas& canvas() const noexcept {
    return mCanvas;
  }

  /**
   * The platform differences this frame can rely on.
   *
   * Always valid: a context built without a snapshot reports a software raster canvas, which is what
   * a hand-built context over a `Bitmap` actually is.
   */
  [[nodiscard]] const RendererCapabilities& capabilities() const noexcept;

  /** Logical-to-physical scale of the frame; text uses it to rasterise at device size. */
  [[nodiscard]] float uiScale() const noexcept {
    return mUiScale;
  }

  /** Physical target size; a custom context can leave it empty and fall back to rectangular clips. */
  [[nodiscard]] const Size& deviceSize() const noexcept {
    return mDeviceSize;
  }

  /**
   * True when this frame can sample a texture another producer owns.
   *
   * A GPU recording canvas can; a software raster canvas cannot, because it has no device to sample
   * from. `ContentView` draws its placeholder in the latter case instead of a hole.
   */
  [[nodiscard]] bool supportsExternalImage() const noexcept;

  /**
   * Records the external-image command at this point in the frame's drawing.
   *
   * The texture stays the producer's: this borrows the view for the frame and copies nothing.
   */
  bool drawContentFrame(const ContentFrame& frame, const Rect& bounds, const Paint& paint);

  /**
   * Routes content frames/fences into the frame packet instead of waiting on the UI thread.
   *
   * `Window` installs this only for the threaded host: `ContentView` records the producer's fence
   * and texture here, and the render thread executes the fence immediately before sampling.
   */
  void setContentSink(std::vector<ContentFrame>* frames,
    std::vector<ContentHost::FrameFence>* fences) noexcept {
    mContentFrames = frames;
    mContentFences = fences;
  }

  [[nodiscard]] bool defersContentFences() const noexcept {
    return mContentFrames != nullptr && mContentFences != nullptr;
  }

  /** Appends a producer frame/fence to the packet sink; no-op for an immediate context. */
  void recordContentFrame(const ContentFrame& frame, ContentHost::FrameFence fence) const;

  /**
   * Draws one line with the window's device-resolution text path.
   *
   * This is the API custom layers should use. The font and coordinates stay logical; the context's
   * `uiScale` selects the device font and the window cache keeps the shaped blob between frames.
   */
  void drawText(const std::string& text, float x, float baseline, const Font& font,
    const Paint& paint);

private:
  std::vector<PlannedItem>* mPlanItems = nullptr;
  mutable Canvas mCanvas;
  float mUiScale = 1.0f;
  Size mDeviceSize{};
  std::shared_ptr<TextRenderCache> mTextCache;
  std::shared_ptr<const RendererCapabilities> mCapabilities;
  std::vector<ContentFrame>* mContentFrames = nullptr;
  std::vector<ContentHost::FrameFence>* mContentFences = nullptr;
};

} // namespace coral::ui

#endif // CORAL_UI_RENDERCONTEXT_H
