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

#include <ui/ContentHost.h>

#include <algorithm>
#include <utility>

namespace coral::ui {

ContentFrameBudget::ContentFrameBudget(const uint32_t maxSlots)
    : mMaxSlots(std::max<uint32_t>(1u, maxSlots)) {}

void ContentFrameBudget::setMaxSlots(const uint32_t maxSlots) {
  const std::lock_guard<std::mutex> lock(mMutex);
  mMaxSlots = std::max<uint32_t>(1u, maxSlots);
}

uint32_t ContentFrameBudget::maxSlots() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return mMaxSlots;
}

uint32_t ContentFrameBudget::inUse() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return mInUse;
}

bool ContentFrameBudget::tryAcquire() {
  const std::lock_guard<std::mutex> lock(mMutex);
  if (mInUse >= mMaxSlots) return false;
  ++mInUse;
  return true;
}

void ContentFrameBudget::release() {
  const std::lock_guard<std::mutex> lock(mMutex);
  if (mInUse > 0) --mInUse;
}

void ContentHost::setMaxInFlight(const uint32_t count) {
  const std::lock_guard<std::mutex> lock(mMutex);
  mMaxInFlight = std::max<uint32_t>(1u, count);
}

uint32_t ContentHost::maxInFlight() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return mMaxInFlight;
}

uint32_t ContentHost::inFlight() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return static_cast<uint32_t>(mReservedSlots.size());
}

std::optional<ContentHost::Slot> ContentHost::acquireSlot() {
  const std::lock_guard<std::mutex> lock(mMutex);
  if (!mActive || mReservedSlots.size() >= mMaxInFlight) return std::nullopt;
  if (mBudget != nullptr && !mBudget->tryAcquire()) return std::nullopt;
  const Slot slot{ mNextSlot++ };
  mReservedSlots.push_back(slot.id);
  return slot;
}

void ContentHost::publish(const Slot slot, const ContentFrame& image, FrameFence fence) {
  FrameAvailableCallback callback;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    if (!slot.isValid() ||
        std::find(mReservedSlots.begin(), mReservedSlots.end(), slot.id) == mReservedSlots.end()) {
      return;
    }
    mImage = image;
    mFence = std::move(fence);
    if (std::find(mPendingSlots.begin(), mPendingSlots.end(), slot.id) == mPendingSlots.end()) {
      mPendingSlots.push_back(slot.id);
    }
    ++mGeneration;
    callback = mFrameAvailable;
  }
  if (callback) callback();
}

void ContentHost::releaseSlot(const Slot slot) {
  std::shared_ptr<ContentFrameBudget> budget;
  bool released = false;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    const auto reserved = std::find(mReservedSlots.begin(), mReservedSlots.end(), slot.id);
    if (reserved != mReservedSlots.end()) {
      mReservedSlots.erase(reserved);
      released = true;
    }
    std::erase(mPendingSlots, slot.id);
    budget = mBudget;
  }
  if (released && budget != nullptr) budget->release();
}

void ContentHost::publish(const ContentFrame& image, FrameFence fence) {
  FrameAvailableCallback callback;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    mImage = image;
    mFence = std::move(fence);
    ++mGeneration;
    // The callback is copied rather than invoked under the lock: it typically posts to the UI thread,
    // and a UI callback that re-entered the host (to read the new generation, say) would deadlock.
    callback = mFrameAvailable;
  }
  if (callback) {
    callback();
  }
}

void ContentHost::waitForLatestFrame() {
  FrameFence fence;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    fence = mFence;
  }
  if (fence) fence();
  releaseConsumedFrame();
}

void ContentHost::releaseConsumedFrame() {
  // The latest frame is now safe to sample; older pending reservations belong to frames the view
  // skipped, so the producer can reuse those slots.
  std::shared_ptr<ContentFrameBudget> budget;
  uint32_t released = 0;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    for (const uint32_t slot: mPendingSlots) {
      if (std::erase(mReservedSlots, slot) > 0) ++released;
    }
    mPendingSlots.clear();
    budget = mBudget;
  }
  if (budget != nullptr) {
    for (uint32_t index = 0; index < released; ++index) budget->release();
  }
}

ContentFrame ContentHost::latest() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return mImage;
}

ContentHost::LatestSample ContentHost::latestSample() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return LatestSample{ mImage, mFence };
}

uint64_t ContentHost::generation() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return mGeneration;
}

void ContentHost::setActive(const bool active) {
  ActiveChangedCallback callback;
  std::shared_ptr<ContentFrameBudget> budget;
  uint32_t released = 0;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    if (mActive == active) return;
    mActive = active;
    callback = mActiveChanged;
    if (!active) {
      released = static_cast<uint32_t>(mReservedSlots.size());
      mReservedSlots.clear();
      mPendingSlots.clear();
      budget = mBudget;
    }
  }
  if (budget != nullptr) {
    for (uint32_t index = 0; index < released; ++index) budget->release();
  }
  if (callback) callback(active);
}

bool ContentHost::active() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return mActive;
}

void ContentHost::setRequestedSize(const Size& size) {
  const std::lock_guard<std::mutex> lock(mMutex);
  mRequestedSize = { size.width > 0.0f ? size.width : 0.0f,
    size.height > 0.0f ? size.height : 0.0f };
}

Size ContentHost::requestedSize() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return mRequestedSize;
}

void ContentHost::setFrameAvailableCallback(FrameAvailableCallback callback) {
  const std::lock_guard<std::mutex> lock(mMutex);
  mFrameAvailable = std::move(callback);
}

void ContentHost::setRecoveryCallback(RecoveryCallback callback) {
  const std::lock_guard<std::mutex> lock(mMutex);
  mRecovery = std::move(callback);
}

void ContentHost::setActiveChangedCallback(ActiveChangedCallback callback) {
  const std::lock_guard<std::mutex> lock(mMutex);
  mActiveChanged = std::move(callback);
}

void ContentHost::setFrameBudget(std::shared_ptr<ContentFrameBudget> budget) {
  std::shared_ptr<ContentFrameBudget> previous;
  uint32_t released = 0;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    // Changing the budget with reservations outstanding would make their release arithmetic
    // ambiguous. Keep the old budget until the producer drains those slots.
    if (!mReservedSlots.empty() && mBudget != budget) return;
    previous = mBudget;
    mBudget = std::move(budget);
    released = static_cast<uint32_t>(mReservedSlots.size());
  }
  if (previous != nullptr && previous != mBudget) {
    for (uint32_t index = 0; index < released; ++index) previous->release();
  }
}

std::shared_ptr<ContentFrameBudget> ContentHost::frameBudget() const {
  const std::lock_guard<std::mutex> lock(mMutex);
  return mBudget;
}

void ContentHost::notifyDeviceRecovered() {
  FrameAvailableCallback available;
  RecoveryCallback recovery;
  std::shared_ptr<ContentFrameBudget> budget;
  uint32_t released = 0;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    mImage = ContentFrame{};
    mFence = {};
    released = static_cast<uint32_t>(mReservedSlots.size());
    mPendingSlots.clear();
    mReservedSlots.clear();
    ++mGeneration;
    available = mFrameAvailable;
    recovery = mRecovery;
    budget = mBudget;
  }
  if (budget != nullptr) {
    for (uint32_t index = 0; index < released; ++index) budget->release();
  }
  if (available) available();
  if (recovery) recovery();
}

void ContentHost::clear() {
  FrameAvailableCallback callback;
  std::shared_ptr<ContentFrameBudget> budget;
  uint32_t released = 0;
  {
    const std::lock_guard<std::mutex> lock(mMutex);
    mImage = ContentFrame{};
    mFence = {};
    released = static_cast<uint32_t>(mReservedSlots.size());
    mPendingSlots.clear();
    mReservedSlots.clear();
    ++mGeneration;
    callback = mFrameAvailable;
    budget = mBudget;
  }
  if (budget != nullptr) {
    for (uint32_t index = 0; index < released; ++index) budget->release();
  }
  // A cleared host is a visible change too: the view has to redraw to show its placeholder.
  if (callback) {
    callback();
  }
}

} // namespace coral::ui
