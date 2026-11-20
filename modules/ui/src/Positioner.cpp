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

#include <ui/Positioner.h>

#include <algorithm>

namespace coral::ui {

namespace {

[[nodiscard]] bool isEmpty(const Rect& rect) {
  return rect.getWidth() <= 0.0f || rect.getHeight() <= 0.0f;
}

[[nodiscard]] float clampAxis(const float value, const float low, const float high) {
  if (high < low) return low;
  return std::clamp(value, low, high);
}

} // namespace

PlacementResult placeFloating(const PlacementRequest& request) {
  PlacementResult result;
  const Size size = request.size;
  const Rect anchor = request.anchor;
  const bool clamp = !isEmpty(request.available);
  const Rect available = request.available;
  const Insets margin = request.margin;

  const auto placeBelow = [&](const bool alignStart) {
    result.bounds = Rect::MakeXYWH(alignStart ? anchor.getLeft() : anchor.getRight() - size.width,
      anchor.getBottom() + request.gap, size.width, size.height);
  };
  const auto placeAbove = [&](const bool alignStart) {
    result.bounds = Rect::MakeXYWH(alignStart ? anchor.getLeft() : anchor.getRight() - size.width,
      anchor.getTop() - request.gap - size.height, size.width, size.height);
  };
  const auto placeCenter = [&]() {
    const float x = clamp ? available.getLeft() + (available.getWidth() - size.width) * 0.5f
                          : anchor.getLeft();
    const float y = clamp ? available.getTop() + (available.getHeight() - size.height) * 0.5f
                          : anchor.getTop();
    result.bounds = Rect::MakeXYWH(x, y, size.width, size.height);
    result.placement = Placement::Center;
  };

  result.placement = request.preferred;
  switch (request.preferred) {
    case Placement::BelowStart:
      placeBelow(true);
      break;
    case Placement::BelowEnd:
      placeBelow(false);
      break;
    case Placement::AboveStart:
      placeAbove(true);
      break;
    case Placement::AboveEnd:
      placeAbove(false);
      break;
    case Placement::RightStart:
      result.bounds = Rect::MakeXYWH(anchor.getRight() + request.gap,
        anchor.getTop() + (anchor.getHeight() - size.height) * 0.5f, size.width, size.height);
      break;
    case Placement::LeftStart:
      result.bounds = Rect::MakeXYWH(anchor.getLeft() - request.gap - size.width,
        anchor.getTop() + (anchor.getHeight() - size.height) * 0.5f, size.width, size.height);
      break;
    case Placement::Center:
      placeCenter();
      return result;
  }

  if (!clamp) return result;

  const float minLeft = available.getLeft() + margin.left;
  const float minTop = available.getTop() + margin.top;
  const float maxRight = available.getRight() - margin.right;
  const float maxBottom = available.getBottom() - margin.bottom;

  // Vertical flip: below <-> above. Only when the opposite side actually fits, so a surface taller
  // than the window keeps the requested side and overflows predictably at the far edge.
  const bool vertical = result.placement == Placement::BelowStart ||
    result.placement == Placement::BelowEnd || result.placement == Placement::AboveStart ||
    result.placement == Placement::AboveEnd;
  if (vertical && result.bounds.getBottom() > maxBottom && result.placement == Placement::BelowStart) {
    if (anchor.getTop() - request.gap - size.height >= minTop) {
      placeAbove(true);
      result.placement = Placement::AboveStart;
      result.flipped = true;
    }
  } else if (vertical && result.bounds.getBottom() > maxBottom &&
             result.placement == Placement::BelowEnd) {
    if (anchor.getTop() - request.gap - size.height >= minTop) {
      placeAbove(false);
      result.placement = Placement::AboveEnd;
      result.flipped = true;
    }
  } else if (vertical && result.bounds.getTop() < minTop && result.placement == Placement::AboveStart) {
    if (anchor.getBottom() + request.gap + size.height <= maxBottom) {
      placeBelow(true);
      result.placement = Placement::BelowStart;
      result.flipped = true;
    }
  } else if (vertical && result.bounds.getTop() < minTop && result.placement == Placement::AboveEnd) {
    if (anchor.getBottom() + request.gap + size.height <= maxBottom) {
      placeBelow(false);
      result.placement = Placement::BelowEnd;
      result.flipped = true;
    }
  }

  // Horizontal flip for side placements (a submenu that would run off the right edge opens left).
  if (result.placement == Placement::RightStart && result.bounds.getRight() > maxRight) {
    if (anchor.getLeft() - request.gap - size.width >= minLeft) {
      result.bounds = Rect::MakeXYWH(anchor.getLeft() - request.gap - size.width,
        result.bounds.getTop(), size.width, size.height);
      result.placement = Placement::LeftStart;
      result.flipped = true;
    }
  } else if (result.placement == Placement::LeftStart && result.bounds.getLeft() < minLeft) {
    if (anchor.getRight() + request.gap + size.width <= maxRight) {
      result.bounds = Rect::MakeXYWH(anchor.getRight() + request.gap, result.bounds.getTop(),
        size.width, size.height);
      result.placement = Placement::RightStart;
      result.flipped = true;
    }
  }

  const float clampedX = clampAxis(result.bounds.getLeft(), minLeft,
    maxRight - size.width);
  const float clampedY = clampAxis(result.bounds.getTop(), minTop,
    maxBottom - size.height);
  if (clampedX != result.bounds.getLeft() || clampedY != result.bounds.getTop()) {
    result.bounds = Rect::MakeXYWH(clampedX, clampedY, size.width, size.height);
    result.shifted = true;
  }
  return result;
}

} // namespace coral::ui
