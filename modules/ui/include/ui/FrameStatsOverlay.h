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

#ifndef CORAL_UI_FRAMESTATSOVERLAY_H
#define CORAL_UI_FRAMESTATSOVERLAY_H

#include <ui/Flex.h>
#include <ui/TextLayer.h>
#include <ui/Window.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace coral::ui {

/** How the overlay remembers its on/off state. */
struct FrameStatsOverlayOptions {
  /**
   * File that stores `enabled=0|1`; empty keeps the state process-local.
   *
   * A development overlay is toggled constantly, so remembering the choice across runs is what makes
   * the toggle usable: a product binds `F12` (or its own command) to `setEnabled` and the next launch
   * starts the way the developer left it.
   */
  std::string settingsPath;
  /** Whether a fresh settings file starts visible. */
  bool enabledByDefault = true;
  /**
   * Frame times kept for the sparkline; 0 disables the graph.
   *
   * The eight text lines say what the current frame costs; a graph says what the *last few seconds*
   * looked like, which is the difference between "this frame is 4 ms" and "every 30th frame is 20 ms".
   * 120 frames is two seconds at 60 Hz.
   */
  uint32_t historyFrames = 120;
  /** Height of the sparkline in logical pixels. */
  float historyHeight = 28.0f;
};

/**
 * A small development overlay showing `Window::frameStatistics()`.
 *
 * The overlay owns a `Flex` column of text lines: frame counts and timings, the render host and its
 * refresh rate, the threaded queue/drop/failure counters, and the current content size. Add
 * `layer()` to the root (or a fixed-size panel) in debug builds. `setStatistics` updates the lines in
 * place; `setEnabled` shows or hides it and, with a settings path, persists that choice.
 */
class FrameStatsOverlay {
 public:
  explicit FrameStatsOverlay(std::shared_ptr<const graphics::Font> font, Color color = Color::White(),
    FrameStatsOverlayOptions options = {});
  ~FrameStatsOverlay();

  FrameStatsOverlay(const FrameStatsOverlay&) = delete;
  FrameStatsOverlay& operator=(const FrameStatsOverlay&) = delete;

  /** The panel size (width x height) the text lines and the sparkline need. */
  [[nodiscard]] Size preferredSize() const noexcept {
    return mPanelSize;
  }

  /** The column to add to the layer tree. */
  [[nodiscard]] const std::shared_ptr<Flex>& layer() const noexcept { return mLayer; }

  /** Subscribes to the window's frame completion and updates the text automatically. */
  void attachTo(Window& window);
  /** Unsubscribes; safe to call more than once. */
  void detach();

  /** Shows or hides the overlay, persisting the choice when a settings path was configured. */
  void setEnabled(bool enabled);
  [[nodiscard]] bool enabled() const noexcept { return mEnabled; }

  /** Re-reads the persisted state (an unreadable or absent file keeps the current state). */
  void loadSettings();

  /** Updates the lines; called from the window's frame callback. */
  void setStatistics(const FrameStatistics& statistics);

  /** @return how many frame times the sparkline currently holds (0 when it is disabled). */
  [[nodiscard]] uint32_t historySampleCount() const noexcept;
  /** @return the largest frame time in the sparkline, in milliseconds (1.0 when it is empty). */
  [[nodiscard]] double historyPeakMilliseconds() const noexcept;
  /** @return the most recent frame time pushed into the sparkline, in milliseconds. */
  [[nodiscard]] double historyLastMilliseconds() const noexcept;

  /** Formats one line; exposed for tests and products that log the same text. */
  [[nodiscard]] static std::string format(const FrameStatistics& statistics);

 private:
  void saveSettings() const;
  void drawHistory(Canvas& canvas, const Rect& bounds) const;

  std::shared_ptr<Flex> mLayer;
  std::shared_ptr<TextLayer> mFrames;
  std::shared_ptr<TextLayer> mAverage;
  std::shared_ptr<TextLayer> mLast;
  std::shared_ptr<TextLayer> mHost;
  std::shared_ptr<TextLayer> mStages;
  std::shared_ptr<TextLayer> mStageFields;
  std::shared_ptr<TextLayer> mStageRecord;
  std::shared_ptr<TextLayer> mDrawSources;
  std::shared_ptr<TextLayer> mPipeline;
  std::shared_ptr<TextLayer> mContent;
  std::shared_ptr<TextLayer> mProcess;
  /** The sparkline layer; null when `historyFrames` is 0. */
  std::shared_ptr<Layer> mGraph;
  Size mPanelSize{};
  /** Ring of frame times in milliseconds, oldest at `mHistoryNext`. */
  std::vector<double> mHistory;
  size_t mHistoryNext = 0;
  double mHistoryPeak = 1.0;
  double mHistoryLast = 0.0;
  FrameStatsOverlayOptions mOptions;
  bool mEnabled = true;
  Window* mWindow = nullptr;
  uint64_t mFrameCallbackId = 0;
  /** Samples the (syscall-backed) process counters every `kProcessSampleInterval` updates. */
  uint32_t mProcessTick = 0;
};

} // namespace coral::ui

#endif // CORAL_UI_FRAMESTATSOVERLAY_H
