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

#include <ui/Overlay.h>

#include <ui/RenderContext.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

OverlayLayer::OverlayLayer() {
  setVisible(false);
  setHitTestable(true);
  setZIndex(10000);
  LayoutParams params;
  params.position = Position::Absolute;
  setLayoutParams(params);
}

OverlayLayer::~OverlayLayer() = default;

uint64_t OverlayLayer::show(std::shared_ptr<Layer> layer, OverlayOptions options) {
  if (layer == nullptr) return 0;
  if (layer->parent() != this) addChild(layer);
  Entry entry;
  entry.id = mNextId++;
  entry.layer = std::move(layer);
  entry.options = options;
  mEntries.push_back(std::move(entry));
  updateInputMode();
  setVisible(true);
  updateFocusScope();
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
  return mEntries.back().id;
}

bool OverlayLayer::dismiss(const uint64_t id) {
  const auto position = std::find_if(mEntries.begin(), mEntries.end(),
    [id](const Entry& entry) { return entry.id == id; });
  if (position == mEntries.end()) return false;
  if (position->layer != nullptr) {
    position->layer->setVisible(false);
    if (mInFrame) {
      mPendingRemoval.push_back(position->layer);
    } else {
      removeChild(position->layer);
    }
  }
  mEntries.erase(position);
  updateInputMode();
  setVisible(!mEntries.empty());
  updateFocusScope();
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
  return true;
}

bool OverlayLayer::setAnchor(const uint64_t id, const Rect& anchor) {
  const auto position = std::find_if(mEntries.begin(), mEntries.end(),
    [id](const Entry& entry) { return entry.id == id; });
  if (position == mEntries.end()) return false;
  position->options.anchor = anchor;
  setNeedsLayout();
  return true;
}

bool OverlayLayer::dismissTop() {
  if (mEntries.empty()) return false;
  return dismiss(mEntries.back().id);
}

void OverlayLayer::dismissAll() {
  if (mEntries.empty()) return;
  for (Entry& entry: mEntries) {
    if (entry.layer != nullptr) {
      entry.layer->setVisible(false);
      if (mInFrame) {
        mPendingRemoval.push_back(entry.layer);
      } else {
        removeChild(entry.layer);
      }
    }
  }
  mEntries.clear();
  updateInputMode();
  setVisible(false);
  updateFocusScope();
  setNeedsLayout();
  setNeedsPaint();
  notifyChanged();
}

Layer* OverlayLayer::topLayer() const noexcept {
  if (mEntries.empty()) return nullptr;
  return mEntries.back().layer.get();
}

uint64_t OverlayLayer::topId() const noexcept {
  return mEntries.empty() ? 0 : mEntries.back().id;
}

bool OverlayLayer::topIsModal() const noexcept {
  return !mEntries.empty() && mEntries.back().options.modal;
}

void OverlayLayer::onFrame(const std::chrono::steady_clock::time_point now) {
  mInFrame = true;
  Layer::onFrame(now);
  mInFrame = false;
  for (const auto& layer: mPendingRemoval) {
    removeChild(layer);
  }
  mPendingRemoval.clear();
}

void OverlayLayer::updateInputMode() {
  bool passThrough = !mEntries.empty();
  for (const Entry& entry: mEntries) {
    if (!entry.options.passThroughPointer) {
      passThrough = false;
      break;
    }
  }
  setHitTestable(!passThrough);
}

void OverlayLayer::updateFocusScope() {
  const bool modal = std::any_of(mEntries.begin(), mEntries.end(),
    [](const Entry& entry) { return entry.options.modal; });
  setFocusScope(modal);
  setNeedsPaint();
}

Size OverlayLayer::measure(const Size& available) {
  Size size = available;
  if (size.width <= 0.0f || size.height <= 0.0f) {
    size = parent() != nullptr ? Size{ parent()->bounds().getWidth(), parent()->bounds().getHeight() }
                               : Size{ 0.0f, 0.0f };
  }
  setMeasuredSize(size);
  return measuredSize();
}

void OverlayLayer::layout() {
  if (parent() != nullptr) setBounds(parent()->bounds());
  const Rect area = bounds();
  for (Entry& entry: mEntries) {
    if (entry.layer == nullptr) continue;
    const Size measured = entry.layer->measure({ area.getWidth(), area.getHeight() });
    if (entry.options.anchor.getWidth() <= 0.0f && entry.options.anchor.getHeight() <= 0.0f &&
        entry.options.preferred == Placement::Center) {
      // A centred surface gets the whole area: dialogs centre their own panel and may draw their
      // own scrim, while the host still owns hit testing and dismissal.
      entry.layer->setBounds(area);
    } else {
      PlacementRequest request;
      request.anchor = entry.options.anchor;
      request.size = measured;
      request.available = area;
      request.preferred = entry.options.anchor.getWidth() > 0.0f ? entry.options.preferred
                                                                 : Placement::Center;
      request.gap = entry.options.gap;
      request.margin = entry.options.margin;
      entry.layer->setBounds(placeFloating(request).bounds);
    }
    entry.layer->layout();
  }
}

void OverlayLayer::drawSelf(RenderContext& context) {
  const bool scrim = std::any_of(mEntries.begin(), mEntries.end(),
    [](const Entry& entry) { return entry.options.modal && entry.options.drawScrim; });
  if (!scrim) return;
  graphics::Paint paint;
  paint.color = mScrim;
  paint.style = graphics::Paint::Style::kFill;
  context.canvas().drawRect(bounds(), paint);
}


bool OverlayLayer::onPointerDown(const PointerEvent& event) {
  // The window already delivered the press to the hit child (an entry is a normal child of this
  // host), so only the miss case is handled here: dismiss the top surface or block the page.
  if (hitTest(event.position) != this || mEntries.empty()) return false;
  if (mEntries.back().options.dismissOnOutsideClick) {
    dismissTop();
  }
  // Either the light dismiss consumed the click, or a non-dismissible modal is blocking the page.
  return true;
}

bool OverlayLayer::onKeyDown(const KeyEvent& event) {
  if (event.key != Key::Escape || mEntries.empty()) return false;
  if (!mEntries.back().options.dismissOnEscape) return false;
  dismissTop();
  return true;
}

} // namespace coral::ui
