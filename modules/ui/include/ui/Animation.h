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

#ifndef CORAL_UI_ANIMATION_H
#define CORAL_UI_ANIMATION_H

#include <ui/Curve.h>

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

namespace coral::ui {

/**
 * Drives one float from `from` to `to` over a fixed duration.
 *
 * The controller is clock-agnostic: the owner feeds the frame delta, which makes tests deterministic
 * and lets a window with variable frame times drive it directly. The value is eased, so it is the
 * animated value, not raw progress; `progress()` exposes the linear fraction for callers that need it.
 */
class AnimationController {
 public:
  AnimationController() = default;

  void start(float from, float to, std::chrono::milliseconds duration,
    std::shared_ptr<const Curve> curve = Curve::easeInOut());
  void stop();

  [[nodiscard]] bool running() const noexcept {
    return mRunning;
  }
  [[nodiscard]] float value() const noexcept {
    return mValue;
  }
  [[nodiscard]] float progress() const noexcept {
    return mProgress;
  }

  void setOnFinished(std::function<void()> callback) {
    mFinished = std::move(callback);
  }

  /** Advances by @p dt and returns the new value; the finished callback fires at most once. */
  float advance(std::chrono::milliseconds dt);

 private:
  float mFrom = 0.0f;
  float mTo = 0.0f;
  float mValue = 0.0f;
  float mProgress = 0.0f;
  std::chrono::milliseconds mElapsed{ 0 };
  std::chrono::milliseconds mDuration{ 0 };
  std::shared_ptr<const Curve> mCurve = Curve::easeInOut();
  bool mRunning = false;
  std::function<void()> mFinished;
};

/**
 * A float that can animate toward a target.
 *
 * `set` is the snap path (model updates, first layout); `animateTo` starts from the current value, so
 * retargeting mid-flight continues from where the animation is instead of jumping. `advance` returns
 * true while the animation is still running, which is what a frame loop uses to keep requesting
 * frames.
 */
class AnimatedFloat {
 public:
  explicit AnimatedFloat(float value = 0.0f) : mValue(value) {}

  [[nodiscard]] float value() const noexcept {
    return mValue;
  }
  void set(float value) {
    mController.stop();
    mFrom = value;
    mTarget = value;
    mValue = value;
  }

  void animateTo(float target, std::chrono::milliseconds duration,
    std::shared_ptr<const Curve> curve = Curve::easeInOut()) {
    if (duration.count() <= 0) {
      set(target);
      return;
    }
    mFrom = mValue;
    mTarget = target;
    mController.start(0.0f, 1.0f, duration, std::move(curve));
  }

  [[nodiscard]] bool running() const noexcept {
    return mController.running();
  }
  void stop() {
    mController.stop();
    mValue = mTarget;
  }

  [[nodiscard]] bool advance(std::chrono::milliseconds dt) {
    if (!mController.running()) return false;
    const float eased = mController.advance(dt);
    mValue = mFrom + (mTarget - mFrom) * eased;
    if (!mController.running()) mValue = mTarget;
    return mController.running();
  }

  void setOnFinished(std::function<void()> callback) {
    mController.setOnFinished(std::move(callback));
  }

 private:
  AnimationController mController;
  float mFrom = 0.0f;
  float mTarget = 0.0f;
  float mValue = 0.0f;
};

} // namespace coral::ui

#endif // CORAL_UI_ANIMATION_H
