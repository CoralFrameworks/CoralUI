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

#include <ui/Curve.h>

#include <algorithm>
#include <cmath>

namespace coral::ui {

namespace {

[[nodiscard]] float bezier(const float t, const float p1, const float p2) {
  const float u = 1.0f - t;
  return 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t;
}

class CubicBezierCurve final : public Curve {
 public:
  CubicBezierCurve(const float x1, const float y1, const float x2, const float y2)
    : mX1(std::clamp(x1, 0.0f, 1.0f)), mY1(y1), mX2(std::clamp(x2, 0.0f, 1.0f)), mY2(y2) {}

  [[nodiscard]] float transform(const float progress) const override {
    if (progress <= 0.0f) return 0.0f;
    if (progress >= 1.0f) return 1.0f;
    // Binary search x(t) = progress: x is monotonic because both control x's are clamped to [0,1].
    float low = 0.0f;
    float high = 1.0f;
    for (int step = 0; step < 24; ++step) {
      const float mid = (low + high) * 0.5f;
      if (bezier(mid, mX1, mX2) < progress) {
        low = mid;
      } else {
        high = mid;
      }
    }
    return bezier((low + high) * 0.5f, mY1, mY2);
  }

 private:
  float mX1;
  float mY1;
  float mX2;
  float mY2;
};

class LinearCurve final : public Curve {
 public:
  [[nodiscard]] float transform(const float progress) const override {
    return std::clamp(progress, 0.0f, 1.0f);
  }
};

} // namespace

std::shared_ptr<const Curve> Curve::linear() {
  static const std::shared_ptr<const Curve> curve = std::make_shared<LinearCurve>();
  return curve;
}

std::shared_ptr<const Curve> Curve::easeIn() {
  static const std::shared_ptr<const Curve> curve = cubicBezier(0.42f, 0.0f, 1.0f, 1.0f);
  return curve;
}

std::shared_ptr<const Curve> Curve::easeOut() {
  static const std::shared_ptr<const Curve> curve = cubicBezier(0.0f, 0.0f, 0.58f, 1.0f);
  return curve;
}

std::shared_ptr<const Curve> Curve::easeInOut() {
  static const std::shared_ptr<const Curve> curve = cubicBezier(0.42f, 0.0f, 0.58f, 1.0f);
  return curve;
}

std::shared_ptr<const Curve> Curve::cubicBezier(const float x1, const float y1, const float x2,
    const float y2) {
  return std::make_shared<CubicBezierCurve>(x1, y1, x2, y2);
}

std::shared_ptr<const Curve> Curve::forEasing(const Easing easing) {
  switch (easing) {
    case Easing::Linear: return linear();
    case Easing::EaseIn: return easeIn();
    case Easing::EaseOut: return easeOut();
    case Easing::EaseInOut: return easeInOut();
  }
  return easeOut();
}

Spring::Spring(const float stiffness, const float damping, const float mass)
    : mStiffness(std::max(stiffness, 0.0f)), mDamping(std::max(damping, 0.0f)),
      mMass(std::max(mass, 0.001f)) {}

void Spring::advance(State& state, const float target, const float dt) const {
  if (dt <= 0.0f) return;
  const float displacement = state.value - target;
  const float force = -mStiffness * displacement - mDamping * state.velocity;
  state.velocity += (force / mMass) * dt;
  state.value += state.velocity * dt;
}

bool Spring::settled(const State& state, const float target, const float epsilon) const {
  return std::abs(state.value - target) < epsilon && std::abs(state.velocity) < epsilon;
}

} // namespace coral::ui
