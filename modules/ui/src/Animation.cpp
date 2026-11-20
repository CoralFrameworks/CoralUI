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

#include <ui/Animation.h>

#include <algorithm>

namespace coral::ui {

void AnimationController::start(const float from, const float to,
    const std::chrono::milliseconds duration, std::shared_ptr<const Curve> curve) {
  mFrom = from;
  mTo = to;
  mValue = from;
  mProgress = 0.0f;
  mElapsed = std::chrono::milliseconds{ 0 };
  mDuration = duration;
  mCurve = curve != nullptr ? std::move(curve) : Curve::linear();
  mRunning = true;
  if (duration.count() <= 0) {
    mValue = to;
    mProgress = 1.0f;
    mRunning = false;
  }
}

void AnimationController::stop() {
  mRunning = false;
}

float AnimationController::advance(const std::chrono::milliseconds dt) {
  if (!mRunning) return mValue;
  mElapsed += std::max(dt, std::chrono::milliseconds{ 0 });
  mProgress = mDuration.count() > 0
    ? std::clamp(static_cast<float>(mElapsed.count()) / static_cast<float>(mDuration.count()),
        0.0f, 1.0f)
    : 1.0f;
  mValue = mFrom + (mTo - mFrom) * mCurve->transform(mProgress);
  if (mProgress >= 1.0f) {
    mRunning = false;
    mValue = mTo;
    if (mFinished) {
      auto finished = std::move(mFinished);
      mFinished = nullptr;
      finished();
    }
  }
  return mValue;
}

} // namespace coral::ui
