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

#ifndef CORAL_UI_CONTENTVIEW_H
#define CORAL_UI_CONTENTVIEW_H

#include <ui/Types.h>

#include <ui/ContentHost.h>
#include <ui/Layer.h>


#include <memory>

namespace coral::ui {

/**
 * A layer that shows the latest frame from a `ContentHost`.
 *
 * The frame's texture belongs to another producer and is sampled through the UI content-frame
 * recording,
 * so the video path stays zero-copy. A frame with no publisher (or a software canvas that cannot
 * sample an external texture) draws a placeholder surface, which is visible rather than a hole.
 * Children draw on top, so a play badge or a title overlay is just another child layer.
 *
 * The view also drives the host's lifecycle: while it is visible it tells the producer how large a
 * target to render (`setRequestedSize`, in device pixels), and when it is hidden it sets `active`
 * to false so the producer can stop. It does not know about invisible ancestors or a minimised
 * window; an application that needs that stops publishing itself.
 */
class ContentView final : public Layer {
 public:
  explicit ContentView(std::shared_ptr<ContentHost> host);

  void setHost(std::shared_ptr<ContentHost> host);
  [[nodiscard]] const std::shared_ptr<ContentHost>& host() const noexcept {
    return mHost;
  }

  void setPlaceholderColor(Color color);
   void setImagePaint(Paint paint);

  /** True while the displayed frame still has the pre-resize size and is being stretched. */
  [[nodiscard]] bool awaitingResize() const noexcept {
    return mAwaitingResize;
  }
  /** The device size of the frame last sampled, zero before the first frame. */
  [[nodiscard]] const Size& frameSize() const noexcept {
    return mFrameSize;
  }

  void draw(RenderContext& context) override;
  void onVisibilityChanged(bool visible) override;
  void onDeviceRecovered() override;

 private:
  /** Requests the target size the view now needs, when it differs from the last request. */
  void updateRequestedSize(float uiScale);

  std::shared_ptr<ContentHost> mHost;
  Paint mImagePaint{};
  Color mPlaceholder = Color::MakeRGBA8(35, 38, 46, 255);
  Size mRequestedSize{};
  Size mFrameSize{};
  bool mAwaitingResize = false;
};

} // namespace coral::ui

#endif // CORAL_UI_CONTENTVIEW_H
