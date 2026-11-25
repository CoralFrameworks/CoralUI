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

#ifndef CORAL_UI_RATING_H
#define CORAL_UI_RATING_H

#include <ui/Types.h>

#include <ui/Layer.h>

#include <functional>
#include <optional>

namespace coral::ui {

struct Theme;

/** A five-star (configurable) rating control. */
class Rating final : public Layer {
 public:
  Rating(int stars = 5, int value = 0);

  void setStars(int stars);
  [[nodiscard]] int stars() const noexcept {
    return mStars;
  }
  void setValue(int value);
  /** Half-step aware rating in [0, stars]. */
  void setRating(double rating);
  [[nodiscard]] double rating() const noexcept {
    return mValue;
  }
  [[nodiscard]] int value() const noexcept;
  void setHalfSteps(bool halfSteps) {
    mHalfSteps = halfSteps;
    commit(mValue);
  }
  [[nodiscard]] bool halfSteps() const noexcept {
    return mHalfSteps;
  }
  void setStarSize(float size);
  void setColors(Color filled, Color empty);
  void setOnChanged(std::function<void(int)> callback) {
    mOnChanged = std::move(callback);
  }
  /** Half-step aware callback; preferred when half steps are enabled. */
  void setOnRatingChanged(std::function<void(double)> callback) {
    mOnRatingChanged = std::move(callback);
  }
  void setReadOnly(bool readOnly) {
    mReadOnly = readOnly;
  }
  [[nodiscard]] bool readOnly() const noexcept {
    return mReadOnly;
  }

  [[nodiscard]] bool focusable() const override {
    return !mReadOnly;
  }
  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerUp(const PointerEvent& event) override;
  bool onKeyDown(const KeyEvent& event) override;
  [[nodiscard]] std::optional<AccessibleRange> accessibleRange() const override;
  bool performAccessibilityAction(AccessibilityAction action,
    std::string_view value) override;
  [[nodiscard]] const char* defaultStyleName() const override {
    return "rating";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  void setFromPoint(Point point);
  void commit(double value);

  Color mFilled = Color::MakeRGBA8(255, 190, 80, 255);
  Color mEmpty = Color::MakeRGBA8(70, 76, 88, 255);
  std::function<void(int)> mOnChanged;
  std::function<void(double)> mOnRatingChanged;
  int mStars = 5;
  double mValue = 0.0;
  float mStarSize = 20.0f;
  float mGap = 4.0f;
  bool mDragging = false;
  bool mReadOnly = false;
  bool mHalfSteps = false;
};

} // namespace coral::ui

#endif // CORAL_UI_RATING_H
