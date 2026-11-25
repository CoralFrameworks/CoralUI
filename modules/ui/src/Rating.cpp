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

#include <ui/Rating.h>

#include <ui/Theme.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace coral::ui {

namespace {
std::string ratingText(const double value) {
  if (std::abs(value - std::round(value)) < 1e-6) {
    return std::to_string(static_cast<int>(std::lround(value)));
  }
  char text[16]{};
  std::snprintf(text, sizeof(text), "%.1f", value);
  return text;
}
}

Rating::Rating(const int stars, const int value)
    : mStars(std::max(1, stars)),
      mValue(std::clamp(value, 0, std::max(1, stars))) {
  setAccessibleRole(AccessibilityRole::Slider);
  setAccessibleName("Rating");
  setAccessibleValue(ratingText(mValue));
}

void Rating::setStars(const int stars) {
  mStars = std::max(1, stars);
  mValue = std::clamp(mValue, 0.0, static_cast<double>(mStars));
  setAccessibleValue(ratingText(mValue));
  setNeedsLayout();
  setNeedsPaint();
}

void Rating::setValue(const int value) {
  commit(static_cast<double>(value));
}

void Rating::setRating(const double rating) {
  commit(rating);
}

int Rating::value() const noexcept {
  return static_cast<int>(std::floor(mValue + 1e-6));
}

void Rating::setStarSize(const float size) {
  mStarSize = std::max(8.0f, size);
  setNeedsLayout();
  setNeedsPaint();
}

void Rating::setColors(const Color filled, const Color empty) {
  mFilled = filled;
  mEmpty = empty;
  setNeedsPaint();
}

Size Rating::measure(const Size& available) {
  (void)available;
  const float width = static_cast<float>(mStars) * mStarSize +
    static_cast<float>(mStars - 1) * mGap;
  setMeasuredSize({ width, mStarSize });
  return measuredSize();
}

void Rating::drawSelf(RenderContext& context) {
  auto& canvas = context.canvas();
  for (int index = 0; index < mStars; ++index) {
    const float left = bounds().getLeft() + static_cast<float>(index) * (mStarSize + mGap);
    const Point center{ left + mStarSize * 0.5f, bounds().getTop() + mStarSize * 0.5f };
    graphics::Path path;
    for (int point = 0; point < 10; ++point) {
      const float radius = (point % 2 == 0 ? 0.5f : 0.21f) * mStarSize;
      const float angle = -1.5707963f + static_cast<float>(point) * 0.6283185f;
      const Point position{ center.x + std::cos(angle) * radius,
        center.y + std::sin(angle) * radius };
      if (point == 0) path.moveTo(position);
      else path.lineTo(position);
    }
    path.close();
    graphics::Paint paint;
    paint.color = mValue >= static_cast<double>(index) + 1.0 ? mFilled : mEmpty;
    paint.style = graphics::Paint::Style::kFill;
    canvas.drawPath(path, paint);
    if (mValue > static_cast<double>(index) + 0.25 &&
        mValue < static_cast<double>(index) + 0.75) {
      canvas.save();
      canvas.clipRect(Rect::MakeXYWH(left, bounds().getTop(), mStarSize * 0.5f,
        mStarSize));
      graphics::Paint half = paint;
      half.color = mFilled;
      canvas.drawPath(path, half);
      canvas.restore();
    }
  }
}

bool Rating::onPointerDown(const PointerEvent& event) {
  if (mReadOnly) return true;
  mDragging = true;
  setFromPoint(event.position);
  return true;
}

bool Rating::onPointerMove(const PointerEvent& event) {
  if (mReadOnly) return true;
  if (mDragging) setFromPoint(event.position);
  return true;
}

bool Rating::onPointerUp(const PointerEvent& event) {
  if (mDragging) setFromPoint(event.position);
  mDragging = false;
  return true;
}

bool Rating::onKeyDown(const KeyEvent& event) {
  if (mReadOnly) return false;
  const double step = mHalfSteps ? 0.5 : 1.0;
  if (event.key == Key::Left) {
    commit(mValue - step);
    return true;
  }
  if (event.key == Key::Right) {
    commit(mValue + step);
    return true;
  }
  if (event.key == Key::Home) {
    commit(0);
    return true;
  }
  if (event.key == Key::End) {
    commit(mStars);
    return true;
  }
  return false;
}

std::optional<AccessibleRange> Rating::accessibleRange() const {
  return AccessibleRange{ mValue, 0.0, static_cast<double>(mStars),
    mHalfSteps ? 0.5 : 1.0 };
}

bool Rating::performAccessibilityAction(const AccessibilityAction action,
    const std::string_view value) {
  if (mReadOnly || action != AccessibilityAction::SetValue) return false;
  try {
    commit(std::stod(std::string(value)));
  } catch (...) {
    return false;
  }
  return true;
}

void Rating::setFromPoint(const Point point) {
  const double relative = static_cast<double>(point.x - bounds().getLeft());
  const double cell = static_cast<double>(mStarSize + mGap);
  const double raw = relative / cell;
  const int index = static_cast<int>(std::floor(raw));
  const double within = raw - static_cast<double>(index);
  commit(static_cast<double>(index) + (mHalfSteps && within < 0.5 ? 0.5 : 1.0));
}

void Rating::commit(const double value) {
  const double step = mHalfSteps ? 0.5 : 1.0;
  const double quantized = std::round(value / step) * step;
  const double clamped = std::clamp(quantized, 0.0, static_cast<double>(mStars));
  if (std::abs(clamped - mValue) < 1e-6) return;
  mValue = clamped;
  setAccessibleValue(ratingText(mValue));
  setNeedsPaint();
  if (mOnChanged) mOnChanged(static_cast<int>(std::floor(mValue)));
  if (mOnRatingChanged) mOnRatingChanged(mValue);
}

void Rating::applyTheme(const Theme& theme) {
  mFilled = theme.controls.accent;
  mEmpty = theme.controls.border;
  setNeedsPaint();
}

} // namespace coral::ui
