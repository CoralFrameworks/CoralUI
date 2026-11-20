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

#ifndef CORAL_UI_HOST_PREVIEWSCHEDULER_H
#define CORAL_UI_HOST_PREVIEWSCHEDULER_H

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace coral::ui::host {

/**
 * The render-thread policy that decides which previews get a frame.
 *
 * This is deliberately independent of `EffectEngine`/`ContentHost`: it schedules *work callbacks*,
 * so a preview session can render an external-drive engine, a thumbnail cache or a test stub through
 * the same policy. The caller invokes `tick()` once per render pass; each callback runs on that
 * thread and owns its own device work.
 *
 * Policy:
 * - hidden previews are skipped and do not consume budget;
 * - due previews are ordered by priority (higher first), then by how overdue they are;
 * - `maxRendersPerTick` caps the total work; the rest are reported as budget-dropped and retried on
 *   the next tick (their next deadline is not advanced);
 * - `targetFps <= 0` means "every tick"; otherwise a preview is due once its interval has elapsed.
 *
 * The scheduler is thread-affine (render thread) and does not lock: sessions are added/removed from
 * that same thread, or behind the host's own command queue.
 */
class PreviewScheduler {
 public:
  using RenderCallback = std::function<void()>;
  using RecoveryCallback = std::function<bool()>;
  using Clock = std::chrono::steady_clock;

  struct Preview {
    uint64_t id = 0;
    RenderCallback render;
    RecoveryCallback recover;
    bool active = true;
    int priority = 0;
    double targetFps = 0.0;
    Clock::time_point due{};
    /** Times this preview was skipped by the budget; dropped previews get the next free slot. */
    uint64_t dropped = 0;
  };

  struct Stats {
    uint64_t ticks = 0;
    uint64_t rendered = 0;
    uint64_t budgetDropped = 0;
    size_t active = 0;
  };

  uint64_t add(RenderCallback render, const double targetFps = 0.0, const int priority = 0,
      RecoveryCallback recover = {}) {
    if (!render) return 0;
    Preview preview;
    preview.id = mNextId++;
    preview.render = std::move(render);
    preview.recover = std::move(recover);
    preview.targetFps = targetFps;
    preview.priority = priority;
    preview.due = Clock::now();
    mPreviews.push_back(std::move(preview));
    return mPreviews.back().id;
  }

  void remove(const uint64_t id) {
    mPreviews.erase(std::remove_if(mPreviews.begin(), mPreviews.end(),
                      [id](const Preview& preview) { return preview.id == id; }),
      mPreviews.end());
  }

  [[nodiscard]] bool setActive(const uint64_t id, const bool active) {
    for (Preview& preview: mPreviews) {
      if (preview.id != id) continue;
      preview.active = active;
      if (active) {
        preview.due = Clock::now();
        preview.dropped = 0;
      }
      return true;
    }
    return false;
  }

  [[nodiscard]] bool setPriority(const uint64_t id, const int priority) {
    for (Preview& preview: mPreviews) {
      if (preview.id != id) continue;
      preview.priority = priority;
      return true;
    }
    return false;
  }

  [[nodiscard]] bool setRecoveryCallback(const uint64_t id, RecoveryCallback recover) {
    for (Preview& preview: mPreviews) {
      if (preview.id != id) continue;
      preview.recover = std::move(recover);
      return true;
    }
    return false;
  }

  [[nodiscard]] bool setTargetFps(const uint64_t id, const double targetFps) {
    for (Preview& preview: mPreviews) {
      if (preview.id != id) continue;
      preview.targetFps = targetFps;
      return true;
    }
    return false;
  }

  /**
   * Rebuilds every active preview through its recovery callback, priority first.
   * This is the session-wide device-loss entry point; callbacks are render-thread affine.
   * @return how many previews recovered successfully.
   */
  size_t recoverAll() {
    std::vector<Preview> active;
    active.reserve(mPreviews.size());
    for (const Preview& preview: mPreviews) {
      if (preview.active && preview.recover) active.push_back(preview);
    }
    std::sort(active.begin(), active.end(), [](const Preview& left, const Preview& right) {
      if (left.priority != right.priority) return left.priority > right.priority;
      return left.due < right.due;
    });
    size_t recovered = 0;
    for (const Preview& candidate: active) {
      if (candidate.recover && candidate.recover()) ++recovered;
    }
    return recovered;
  }

  /** Zero means unlimited; one is the usual "render the most important preview" mode. */
  void setMaxRendersPerTick(const size_t count) noexcept {
    mMaxRendersPerTick = count;
  }

  [[nodiscard]] size_t size() const noexcept {
    return mPreviews.size();
  }

  [[nodiscard]] Stats stats() const noexcept {
    return mStats;
  }

  /**
   * Runs the previews that are due at @p now.
   *
   * A callback may remove/set other previews; the due list is snapshotted first, and an entry that
   * disappeared before its turn is skipped by id.
   */
  void tick(const Clock::time_point now = Clock::now()) {
    ++mStats.ticks;
    mStats.active = 0;
    std::vector<Preview> due;
    due.reserve(mPreviews.size());
    for (Preview& preview: mPreviews) {
      if (!preview.active) continue;
      ++mStats.active;
      if (now < preview.due) continue;
      due.push_back(preview);
    }
    const bool limited = mMaxRendersPerTick != 0;
    std::sort(due.begin(), due.end(), [limited](const Preview& left, const Preview& right) {
      // Under a budget, a preview already skipped runs before fresh work, and the one skipped most
      // often goes first, so a slow high-priority preview cannot starve the others tick after tick.
      // With unlimited budget every due preview runs, so priority alone defines the order.
      if (limited && left.dropped != right.dropped) return left.dropped > right.dropped;
      if (left.priority != right.priority) return left.priority > right.priority;
      return left.due < right.due;
    });

    size_t executed = 0;
    for (const Preview& candidate: due) {
      if (mMaxRendersPerTick != 0 && executed >= mMaxRendersPerTick) {
        const auto skipped = std::find_if(mPreviews.begin(), mPreviews.end(),
          [id = candidate.id](const Preview& preview) { return preview.id == id; });
        if (skipped != mPreviews.end()) ++skipped->dropped;
        ++mStats.budgetDropped;
        continue;
      }
      // The callback may invalidate the vector; look the id up again before running it.
      const uint64_t id = candidate.id;
      const auto found = std::find_if(mPreviews.begin(), mPreviews.end(),
        [id](const Preview& candidate) { return candidate.id == id; });
      if (found == mPreviews.end() || !found->active) continue;
      if (found->targetFps > 0.0) {
        const auto interval = std::chrono::duration_cast<Clock::duration>(
          std::chrono::duration<double>(1.0 / found->targetFps));
        found->due = now + interval;
      } else {
        found->due = now;
      }
      found->render();
      found->dropped = 0;
      ++mStats.rendered;
      ++executed;
    }
  }

  void clear() noexcept {
    mPreviews.clear();
    mStats = {};
  }

 private:
  std::vector<Preview> mPreviews;
  size_t mMaxRendersPerTick = 0;
  uint64_t mNextId = 1;
  Stats mStats;
};

} // namespace coral::ui::host

#endif // CORAL_UI_HOST_PREVIEWSCHEDULER_H
