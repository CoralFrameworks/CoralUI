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

#ifndef CORAL_UI_RECYCLINGLISTVIEW_H
#define CORAL_UI_RECYCLINGLISTVIEW_H

#include <ui/Reconciler.h>
#include <ui/ScrollView.h>

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace coral::ui {

class Window;

/**
 * A virtualized list whose visible rows are real Layer subtrees built from Elements.
 *
 * Unlike `ListView`, which paints text rows itself, this owns one reconciler per row. Only the rows
 * intersecting the viewport are attached to the content layer; a row that scrolls out is detached and
 * kept in a keyed pool, then reattached (same layer identity) when its key becomes visible again.
 * That is the per-row keyed recycling path the Element DSL needs.
 *
 * The row builder must return an element with a stable key. The builder receives the key and the
 * current index, so it can choose a Component/When tree from the view model.
 *
 * Thread affinity: UI thread, like every Layer.
 */
class RecyclingListView final : public ScrollView {
public:
  using RowState = RecyclingRowState;
  using RowBuilder = std::function<ElementPtr(const std::string& key, size_t index,
    const RowState& state)>;

  RecyclingListView(RowBuilder builder, float rowHeight = 30.0f);
  ~RecyclingListView() override;

  RecyclingListView(const RecyclingListView&) = delete;
  RecyclingListView& operator=(const RecyclingListView&) = delete;

  void setRows(std::vector<std::string> keys);
  /**
   * Fades a row in when its layer is first attached; 0 disables the effect.
   *
   * Removal is immediate (the layer returns to the pool). The effect is a property animation, so it
   * runs on the window frame clock and keeps the frame loop alive while it lasts.
   */
  void setRowTransition(std::chrono::milliseconds duration);
  [[nodiscard]] std::chrono::milliseconds rowTransition() const noexcept {
    return mRowTransition;
  }
  /**
   * Duration for a row that moves because something above it changed; negative keeps using
   * `rowTransition`. A 0 makes reorders snap while the enter/exit fades keep their own duration.
   */
  void setRowMoveTransition(std::chrono::milliseconds duration);
  /** Keeps the row at the viewport top anchored when heights above it change; on by default. */
  void setScrollAnchoring(bool enabled);
  [[nodiscard]] bool scrollAnchoring() const noexcept {
    return mScrollAnchoring;
  }
  [[nodiscard]] std::chrono::milliseconds rowMoveTransition() const noexcept {
    return mRowMoveTransition;
  }
  void setRowBuilder(RowBuilder builder);

  /**
   * Selection/focus/hover are stored by key, not by layer: a row that scrolls out is recycled and
   * the state is applied again when the key scrolls back. The builder receives it so the row can
   * paint the right state without retaining a pointer to a detached layer.
   */
  void setSelectedKey(std::string key);
  [[nodiscard]] const std::string& selectedKey() const noexcept { return mSelectedKey; }
  /** Called after the selection changed, including when a bridge selects a row. */
  void setOnSelectedChanged(std::function<void(const std::string&)> callback);
  void setFocusedKey(std::string key);
  [[nodiscard]] const std::string& focusedKey() const noexcept { return mFocusedKey; }
  void setHoveredKey(std::string key);
  [[nodiscard]] const std::string& hoveredKey() const noexcept { return mHoveredKey; }

  /**
   * Optional bridge to the Window focus system. Called when the focused key is reattached, because a
   * detached layer cannot remain the window's focused layer.
   */
  void setFocusRequester(std::function<void(Layer*)> requester);

  /**
   * Binds focus restoration to a real `Window`.
   *
   * When a focused row is detached, the window's focus is cleared so it cannot retain a pointer into
   * the recycle pool. The row and the exact focused descendant are remembered; when the key returns,
   * `focusLayer()` is called with the descendant (or the row if the descendant no longer exists).
   * The window must outlive this view (or call `setFocusOwner(nullptr)` first).
   */
  void setFocusOwner(Window* window);

  /** Deferred-focus state used by tests and diagnostics. */
  [[nodiscard]] bool focusDetached() const noexcept { return mFocusDetached; }

  /**
   * Detached-row pool policy.
   *
   * `distanceWeight` in [0, 1] blends distance from the viewport with recency:
   * - 1 = distance only;
   * - 0 = pure LRU;
   * - intermediate = normalized distance and age.
   *
   * `minRetainAbove` / `minRetainBelow` keep at least that many rows immediately above / below
   * the viewport; `minRetainAbovePixels` / `minRetainBelowPixels` express the same guarantee in
   * layout pixels. A row-count and a pixel value can be combined: the effective count is the max
   * of the two (pixels are converted with the current row pitch), so a caller can say "keep one row
   * or 24px, whichever is larger".
   *
   * They are independent: a list scrolled down can keep a few rows above for a small reversal and a
   * larger below-window for a fast scroll forward. The sum is clamped to `limit` (above is reserved
   * first), so the pool cap still holds. `limit == 0` disables the pool.
   */
  struct PoolPolicy {
    size_t limit = 64;
    size_t minRetainAbove = 0;
    size_t minRetainBelow = 0;
    float minRetainAbovePixels = 0.0f;
    float minRetainBelowPixels = 0.0f;
    double distanceWeight = 1.0;
  };

  void setPoolLimit(size_t limit);
  void setPoolPolicy(PoolPolicy policy);
  [[nodiscard]] const PoolPolicy& poolPolicy() const noexcept { return mPoolPolicy; }
  [[nodiscard]] size_t poolLimit() const noexcept { return mPoolPolicy.limit; }
  [[nodiscard]] uint64_t poolHits() const noexcept { return mPoolHits; }
  [[nodiscard]] uint64_t poolEvictions() const noexcept { return mPoolEvictions; }

  [[nodiscard]] const std::vector<std::string>& rows() const noexcept {
    return mRows;
  }

  [[nodiscard]] float contentHeight() const noexcept {
    return mRowOffsets.empty() ? 0.0f : mRowOffsets.back();
  }

  void setRowHeight(float height);

  [[nodiscard]] float rowHeight() const noexcept {
    return mRowHeight;
  }

  /**
   * Optional per-row height (in layout pixels). Without a provider every row uses rowHeight().
   *
   * Heights are captured into a prefix-sum table on setRows()/setRowHeight()/provider changes;
   * virtualization, scroll extents, hit testing and pixel retention all use the resulting offsets
   * instead of a fixed pitch.
   */
  using RowHeightProvider = std::function<float(const std::string& key, size_t index)>;
  void setRowHeightProvider(RowHeightProvider provider);
  [[nodiscard]] float rowHeightAt(size_t index) const;

  /**
   * Measures mounted row layers and caches their intrinsic heights.
   *
   * A provider still wins when set. Without a provider, auto mode replaces the uniform estimate
   * with `row.measure({contentWidth, 0}).height` as rows mount; new rows start at rowHeight() until
   * their first measurement, so the cached prefix sums gradually replace the estimate. Heights are
   * measured per key, and the visible range is re-synced after a measurement changes an offset.
   */
  void setAutoRowHeight(bool enabled);
  [[nodiscard]] bool autoRowHeight() const noexcept { return mAutoRowHeight; }
  /** Forces the next layout to re-measure visible rows (after model content changes that affect height). */
  void invalidateRowHeights();

  /** Rows above/below the viewport to build-and-measure ahead of scrolling. Default 1. */
  void setMeasurePrefetchRows(size_t rows);
  [[nodiscard]] size_t measurePrefetchRows() const noexcept { return mMeasurePrefetchRows; }
  /** Extra rows (beyond the base) that a fast scroll may prefetch; default 4. */
  void setMaxMeasurePrefetchExtra(size_t rows);
  [[nodiscard]] size_t maxMeasurePrefetchExtra() const noexcept { return mMaxPrefetchExtra; }
  /** Base prefetch plus the current scroll-speed boost. */
  [[nodiscard]] size_t effectiveMeasurePrefetchRows() const noexcept {
    return mMeasurePrefetchRows + mPrefetchBoost;
  }

  /** Number of row measure() calls, useful for regression tests and diagnostics. */
  [[nodiscard]] uint64_t measureCount() const noexcept { return mMeasureCount; }
  /** Estimated platform-clock rate against steady time (1.0 = nominal). */
  [[nodiscard]] double clockRate() const noexcept { return mClockRate; }
  /** The same estimate in estimated ppm; this is not a driver-reported ppm. */
  [[nodiscard]] double clockRatePpm() const noexcept { return (mClockRate - 1.0) * 1'000'000.0; }

  /**
   * Optional driver/host ppm prior. The estimator starts here and stays within ±5% of it.
   * This lets a platform backend supply a calibrated ppm without the list guessing the absolute
   * clock frequency.
   */
  void setClockRateCalibrationPpm(double ppm);
  [[nodiscard]] double clockRateCalibrationPpm() const noexcept { return mCalibratedPpm; }

  /**
   * Optional platform provider for the driver/host ppm estimate.
   *
   * A backend that can query the real display/input clock installs this; the list re-seeds its
   * rate prior on each timestamp event. Without it the rate remains an EWMA estimate.
   */
  using ClockRateProvider = std::function<double()>;
  void setClockRateProvider(ClockRateProvider provider);

  [[nodiscard]] const ClockRateProvider& clockRateProvider() const noexcept {
    return mClockRateProvider;
  }

  /** Rows currently attached to the content layer. */
  [[nodiscard]] size_t mountedRows() const noexcept {
    return mMounted.size();
  }

  /** Detached rows waiting for their key to scroll back into view. */
  [[nodiscard]] size_t recycledRows() const noexcept {
    return mPool.size();
  }

  /** The layer for @p key if it is mounted or pooled; useful for tests and diagnostics. */
  [[nodiscard]] std::shared_ptr<Layer> rowLayer(const std::string& key) const;

  void layout() override;
  bool onScroll(const ScrollEvent& event) override;

  [[nodiscard]] bool focusable() const override {
    return true;
  }
  /** Arrow/Home/End/Page keys move the focused key; Enter/Space activate it. */
  bool onKeyDown(const KeyEvent& event) override;
  void onFrame(std::chrono::steady_clock::time_point now) override;
  void setOnActivate(std::function<void(const std::string&)> callback);
  /** Scrolls so the row for @p key is fully visible; an unknown key is a no-op. */
  void ensureKeyVisible(const std::string& key);
  /** Focuses @p key and scrolls it into view. */
  void scrollToKey(const std::string& key);

  /**
   * Marks keys that act as sticky section headers.
   *
   * The list only computes the sticky state; drawing the pinned band is the owner's job (a small
   * overlay row), because the header usually needs to repeat the group's summary rather than the
   * scrolled row. `stickyHeaderKey` is the nearest sticky row at or above the viewport top, and
   * `stickyHeaderHeight` is that row's height.
   */
  void setStickyKeys(std::vector<std::string> keys);
  [[nodiscard]] const std::vector<std::string>& stickyKeys() const noexcept {
    return mStickyKeys;
  }
  [[nodiscard]] std::string stickyHeaderKey() const;
  [[nodiscard]] float stickyHeaderHeight() const;
  void applyTheme(const Theme& theme) override;
  bool onPointerDown(const PointerEvent& event) override;
  bool onPointerMove(const PointerEvent& event) override;
  bool onPointerLeave() override;

private:
  class Content;

  struct Slot {
    std::string key;
    size_t index = 0;
    bool measureDirty = true;
    uint64_t fontRevision = 0;
    ElementPtr element;
    std::unique_ptr<Reconciler> reconciler;
    std::shared_ptr<Layer> layer;
  };

  void syncRows();
  /** Starts the fade-out for a row that left the model instead of dropping it immediately. */
  void beginExit(const std::shared_ptr<Layer>& layer);
  /** Keeps the pinned header layer in sync with the sticky key and scroll offset. */
  void updateStickyLayer();
  void moveFocus(int delta);
  void removeMissingRows();

  struct PoolMeta {
    uint64_t lastAccess = 0;
    size_t lastIndex = 0;
  };

  void rebuildRowMetrics();
  bool measureVisibleRows(float contentWidth);
  [[nodiscard]] static uint64_t fontRevisionOf(const Layer& layer);

  struct ScrollSample {
    std::chrono::steady_clock::time_point time;
    float distance = 0.0f;
  };

  void refreshPrefetchBoost(std::chrono::steady_clock::time_point now);
  void trimPool();
  void touchPoolEntry(const std::string& key, size_t index);
  [[nodiscard]] size_t countForPixelBudget(bool above, float budget) const;
  [[nodiscard]] Slot* findSlot(const std::string& key) const;
  [[nodiscard]] RowState stateFor(const std::string& key) const;
  [[nodiscard]] const std::string* keyAt(const Point& position) const;
  void detachFocus(const std::string& key, const std::shared_ptr<Layer>& row);
  void restoreFocus(const std::string& key, const std::shared_ptr<Layer>& row);
  [[nodiscard]] static bool isWithin(const Layer* layer, const Layer* ancestor);

  RowBuilder mBuilder;
  RowHeightProvider mHeightProvider;
  bool mAutoRowHeight = false;
  size_t mMeasurePrefetchRows = 1;
  size_t mMaxPrefetchExtra = 4;
  size_t mPrefetchBoost = 0;
  std::deque<ScrollSample> mScrollSamples;
  std::chrono::steady_clock::time_point mTimestampBasePlatform{};
  std::chrono::steady_clock::time_point mTimestampBaseSteady{};
  std::chrono::steady_clock::duration mClockDrift{};
  double mClockRate = 1.0;
  double mCalibratedPpm = 0.0;
  ClockRateProvider mClockRateProvider;
  bool mUsePlatformClock = true;
  std::chrono::steady_clock::time_point mLastPlatformSample{};
  std::chrono::steady_clock::time_point mLastSteadySample{};
  bool mHasClockSample = false;
  bool mHasTimestampBase = false;
  uint64_t mMeasureCount = 0;
  float mMeasuredWidth = 0.0f;
  int mSyncDepth = 0;
  std::unordered_map<std::string, float> mIntrinsicHeights;
  std::vector<std::string> mRows;
  std::vector<std::string> mStickyKeys;
  std::vector<std::shared_ptr<Layer>> mExiting;
  /** Last laid-out rectangle per mounted key, used to animate reorders and height changes. */
  std::unordered_map<std::string, Rect> mLastRowBounds;
  std::shared_ptr<Layer> mStickyLayer;
  std::string mStickyBuiltKey;
  std::unique_ptr<Reconciler> mStickyReconciler;
  std::vector<float> mRowHeights;
  /** Prefix sums; size = rows + 1. */
  std::vector<float> mRowOffsets;
  std::unordered_map<std::string, std::unique_ptr<Slot>> mMounted;
  std::unordered_map<std::string, std::unique_ptr<Slot>> mPool;
  std::unordered_map<std::string, PoolMeta> mPoolMeta;
  uint64_t mAccessTick = 0;
  uint64_t mPoolHits = 0;
  uint64_t mPoolEvictions = 0;
  std::function<void(Layer*)> mFocusRequester;
  Window* mFocusOwner = nullptr;
  Layer* mFocusedDescendant = nullptr;
  bool mFocusDetached = false;
  std::string mSelectedKey;
  std::function<void(const std::string&)> mOnSelectedChanged;
  std::string mFocusedKey;
  std::string mHoveredKey;
  std::shared_ptr<Content> mContent;
  float mRowHeight = 30.0f;
  std::chrono::milliseconds mRowTransition{ 0 };
  std::chrono::milliseconds mRowMoveTransition{ -1 };
  bool mScrollAnchoring = true;
  /** Set when scroll anchoring already compensated the viewport, so rows must not animate too. */
  bool mSuppressRowMove = false;
  std::function<void(const std::string&)> mOnActivate;
  PoolPolicy mPoolPolicy;
  size_t mVisibleFirst = 0;
  size_t mVisibleLast = 0;
};

} // namespace coral::ui

#endif // CORAL_UI_RECYCLINGLISTVIEW_H
