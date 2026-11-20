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

#include <ui/ContentView.h>

#include <cmath>
#include <utility>

namespace coral::ui {

ContentView::ContentView(std::shared_ptr<ContentHost> host) : mHost(std::move(host)) {
  mImagePaint.color = graphics::Color::White();
  mImagePaint.style = graphics::Paint::Style::kFill;
}

void ContentView::setHost(std::shared_ptr<ContentHost> host) {
  mHost = std::move(host);
  mRequestedSize = {};
  if (mHost != nullptr) {
    mHost->setActive(visible());
  }
  setNeedsPaint();
}

void ContentView::setPlaceholderColor(const graphics::Color color) {
  mPlaceholder = color;
  setNeedsPaint();
}

void ContentView::setImagePaint(graphics::Paint paint) {
  mImagePaint = paint;
  setNeedsPaint();
}

void ContentView::updateRequestedSize(const float uiScale) {
  if (mHost == nullptr) return;
  const Size wanted{ bounds().getWidth() * uiScale, bounds().getHeight() * uiScale };
  if (wanted.width == mRequestedSize.width && wanted.height == mRequestedSize.height) return;
  mRequestedSize = wanted;
  mAwaitingResize = true;
  mHost->setRequestedSize(wanted);
}

void ContentView::onVisibilityChanged(const bool visible) {
  if (mHost != nullptr) {
    // A hidden preview asks its producer to stop; showing it again resumes. The host's `active` flag
    // is what a decoder or effect engine polls, so nothing is torn down for a tab switch.
    mHost->setActive(visible);
  }
}

void ContentView::onDeviceRecovered() {
  // The published handle names a texture on the old device. Invalidate it and let the producer/engine
  // rebuild; `notifyDeviceRecovered` invokes that recovery callback after the frame is cleared.
  if (mHost != nullptr) mHost->notifyDeviceRecovered();
  Layer::onDeviceRecovered();
}

void ContentView::draw(RenderContext& context) {
  if (!visible()) {
    return;
  }
  updateRequestedSize(context.uiScale());
  auto& canvas = context.canvas();
  if (clips()) {
    canvas.save();
    canvas.clipRect(bounds());
  }
  // On the immediate path the fence runs here. On the threaded path it is recorded into the frame
  // packet and executed by the render thread immediately before sampling, so a producer's ring slot
  // that is still being written never blocks the UI thread.
  ContentFrame frame;
  if (mHost != nullptr) {
    if (context.defersContentFences()) {
      ContentHost::LatestSample sample = mHost->latestSample();
      frame = sample.frame;
      if (frame.valid()) {
        const std::shared_ptr<ContentHost> host = mHost;
        context.recordContentFrame(frame, [host, fence = std::move(sample.fence)] {
          if (fence) fence();
          host->releaseConsumedFrame();
        });
      }
    } else {
      mHost->waitForLatestFrame();
      frame = mHost->latest();
    }
  }
  if (frame.valid()) {
    mFrameSize = { static_cast<float>(frame.width()), static_cast<float>(frame.height()) };
    // The old frame is intentionally stretched until the producer publishes a frame at the
    // requested size; that is what avoids a black frame during a resize.
    mAwaitingResize = frame.width() != static_cast<uint32_t>(std::lround(mRequestedSize.width)) ||
                      frame.height() != static_cast<uint32_t>(std::lround(mRequestedSize.height));
  }
  if (frame.valid() && context.supportsExternalImage() &&
      context.drawContentFrame(frame, bounds(), mImagePaint)) {
    // The frame is in the recording; children overlay it.
  } else {
    graphics::Paint placeholder;
    placeholder.color = mPlaceholder;
    placeholder.style = graphics::Paint::Style::kFill;
    canvas.drawRect(bounds(), placeholder);
  }
  drawChildren(context);
  if (clips()) {
    canvas.restore();
  }
}

} // namespace coral::ui
