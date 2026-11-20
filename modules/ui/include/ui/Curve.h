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

#ifndef CORAL_UI_CURVE_H
#define CORAL_UI_CURVE_H

#include <cstdint>
#include <memory>

namespace coral::ui {

/** Easing for the small property-animation helper; kept for API compatibility. */
enum class Easing : uint8_t {
  Linear,
  EaseIn,
  EaseOut,
  EaseInOut,
};

/**
 * A normalised timing curve: maps progress in [0,1] to animation progress.
 *
 * Curves are immutable and shared, which is what lets many layers use the same preset without copying
 * it. The built-in presets are cubic beziers with the standard CSS control points; a custom curve can
 * be built from any four control points. Output is not clamped, so an overshoot curve is expressible;
 * callers that need a bounded result clamp it themselves.
 */
class Curve {
 public:
  virtual ~Curve() = default;

  [[nodiscard]] virtual float transform(float progress) const = 0;

  [[nodiscard]] static std::shared_ptr<const Curve> linear();
  [[nodiscard]] static std::shared_ptr<const Curve> easeIn();
  [[nodiscard]] static std::shared_ptr<const Curve> easeOut();
  [[nodiscard]] static std::shared_ptr<const Curve> easeInOut();
  /** A cubic bezier through (0,0), (@p x1,@p y1), (@p x2,@p y2), (1,1). */
  [[nodiscard]] static std::shared_ptr<const Curve> cubicBezier(float x1, float y1, float x2,
    float y2);
  /** Maps the legacy `Easing` enum onto the same engine, so old setters and new styles agree. */
  [[nodiscard]] static std::shared_ptr<const Curve> forEasing(Easing easing);
};

/**
 * A damped spring integrator (semi-implicit Euler).
 *
 * Springs are time-based rather than progress-based: the caller owns the state and feeds a frame delta,
 * which makes them usable for interrupts (retargeting mid-motion) where a fixed-duration curve cannot
 * express the current velocity.
 */
class Spring {
 public:
  struct State {
    float value = 0.0f;
    float velocity = 0.0f;
  };

  Spring(float stiffness = 170.0f, float damping = 26.0f, float mass = 1.0f);

  /** Advances @p state by @p dt seconds toward @p target. */
  void advance(State& state, float target, float dt) const;
  [[nodiscard]] bool settled(const State& state, float target, float epsilon = 0.001f) const;

 private:
  float mStiffness;
  float mDamping;
  float mMass;
};

} // namespace coral::ui

#endif // CORAL_UI_CURVE_H
