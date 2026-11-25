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

#ifndef CORAL_UI_SKELETON_H
#define CORAL_UI_SKELETON_H

#include <chrono>
#include <ui/Types.h>

#include <ui/Layer.h>

namespace coral::ui {

struct Theme;

/** A pulsing placeholder used while data is loading. */
class Skeleton final : public Layer {
 public:
  enum class Shape {
    Rounded,
    Circle,
  };

  Skeleton(float width = 160.0f, float height = 14.0f);

  void setSize(float width, float height);
  void setColors(Color color);
  void setLines(int lines);
  [[nodiscard]] int lines() const noexcept {
    return mLines;
  }
  void setLineGap(float gap);
  void setShape(Shape shape);
  [[nodiscard]] Shape shape() const noexcept {
    return mShape;
  }
  void setAnimating(bool animating);
  [[nodiscard]] bool animating() const noexcept {
    return mAnimating;
  }

  Size measure(const Size& available) override;
  void drawSelf(RenderContext& context) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  [[nodiscard]] bool hasRunningAnimation() const override {
    return mAnimating;
  }
  [[nodiscard]] const char* defaultStyleName() const override {
    return "skeleton";
  }

 protected:
  void applyTheme(const Theme& theme) override;

 private:
  Color mColor = Color::MakeRGBA8(52, 56, 66, 255);
  float mWidth = 160.0f;
  float mHeight = 14.0f;
  float mPhase = 0.0f;
  float mLineGap = 6.0f;
  int mLines = 1;
  bool mAnimating = true;
  Shape mShape = Shape::Rounded;
};

} // namespace coral::ui

#endif // CORAL_UI_SKELETON_H
