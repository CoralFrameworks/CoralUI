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

#include <ui/FrameStatsOverlay.h>

#include <ui/ProcessStats.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

namespace coral::ui {

namespace {

/** Process counters cost a few syscalls, so the overlay samples them about twice a second at 60 Hz. */
constexpr uint32_t kProcessSampleInterval = 30;

std::string millisecondsLine(const char* name, const double value) {
  char buffer[96];
  std::snprintf(buffer, sizeof(buffer), "%s %.2f ms", name, value);
  return buffer;
}

const char* hostName(const FrameStatistics& statistics) {
  return statistics.hostKind == RenderHostKind::RenderThread ? "render-thread" : "same-thread";
}

/**
 * The sparkline layer: it owns no samples of its own, it asks the overlay to draw them.
 *
 * Drawing has to read the overlay`s ring buffer, and a callback is the smallest way to say that: the
 * layer stays a plain `Layer`, the overlay keeps one copy of the data, and the two cannot disagree.
 */
class FrameTimeGraph final : public Layer {
 public:
  explicit FrameTimeGraph(std::function<void(coral::ui::Canvas&, const Rect&)> draw,
      const float height)
      : mDraw(std::move(draw)), mHeight(height) {}

  // The base measure reports the layer own bounds, which are zero for a layer that has never been
  // laid out; the graph has to ask for its configured height.
  [[nodiscard]] Size measure(const Size& available) override {
    const Size measured = Layer::measure(available);
    return Size{ measured.width, mHeight };
  }

 protected:
  void drawSelf(RenderContext& context) override {
    if (mDraw != nullptr) mDraw(context.canvas(), bounds());
  }

 private:
  std::function<void(coral::ui::Canvas&, const Rect&)> mDraw;
  float mHeight = 0.0f;
};

} // namespace

FrameStatsOverlay::FrameStatsOverlay(std::shared_ptr<const graphics::Font> font, const Color color,
    FrameStatsOverlayOptions options)
    : mOptions(std::move(options)), mEnabled(mOptions.enabledByDefault) {
  mLayer = std::make_shared<Flex>(Axis::Column);
  mFrames = std::make_shared<TextLayer>("frames 0", font, color);
  mAverage = std::make_shared<TextLayer>("avg 0.00 ms", font, color);
  mLast = std::make_shared<TextLayer>("last 0.00 ms", font, color);
  mHost = std::make_shared<TextLayer>("present 0.00 ms  immediate", font, color);
  mStages = std::make_shared<TextLayer>("render 0.00 ms  submit 0.00 ms", font, color);
  mStageFields = std::make_shared<TextLayer>("stages fields 0.00  place 0.00  pre 0.00  targets 0.00",
    font, color);
  mStageRecord = std::make_shared<TextLayer>("stages arena 0.00  upload 0.00  record 0.00  execute 0.00",
    font, color);
  mDrawSources = std::make_shared<TextLayer>("draws rect 0.00  grad 0.00  image 0.00  path 0.00  field 0.00",
    font, color);
  mPipeline = std::make_shared<TextLayer>("queue 0  drops 0  fails 0  recoveries 0", font, color);
  mContent = std::make_shared<TextLayer>("layers 0 (0 visible)", font, color);
  mProcess = std::make_shared<TextLayer>("mem 0.0 MiB  private 0.0 MiB  handles 0", font, color);
  // The text layers measure one line shorter than they draw (the drawn line uses the font line height),
  // so a column without a minimum per line overlaps the lines. Pin every line to the drawn height.
  const float lineHeight = font != nullptr ? font->metrics().ascent + font->metrics().descent : 16.0f;
  // Absolute lines at a fixed pitch: the column layout measures a text line shorter than it draws, so
  // a flow column overlaps them. An absolute child is laid out from left/top and keeps the pitch.
  const float linePitch = lineHeight + 2.0f;
  int lineIndex = 0;
  for (const auto& line : { mFrames, mAverage, mLast, mHost, mStages, mStageFields, mStageRecord,
         mDrawSources, mPipeline, mContent, mProcess }) {
    LayoutParams lineParams;
    lineParams.position = Position::Absolute;
    lineParams.hasLeft = true;
    lineParams.left = 8.0f;
    lineParams.hasTop = true;
    lineParams.top = 8.0f + static_cast<float>(lineIndex++) * linePitch;
    lineParams.minHeight = linePitch;
    lineParams.maxHeight = linePitch;
    line->setLayoutParams(lineParams);
  }
  mLayer->addChild(mFrames);
  mLayer->addChild(mAverage);
  mLayer->addChild(mLast);
  mLayer->addChild(mHost);
  mLayer->addChild(mStages);
  mLayer->addChild(mStageFields);
  mLayer->addChild(mStageRecord);
  mLayer->addChild(mDrawSources);
  mLayer->addChild(mPipeline);
  mLayer->addChild(mContent);
  mLayer->addChild(mProcess);
  if (mOptions.historyFrames > 0) {
    mHistory.assign(mOptions.historyFrames, 0.0);
    auto graph = std::make_shared<FrameTimeGraph>(
      [this](Canvas& canvas, const Rect& bounds) { drawHistory(canvas, bounds); },
      mOptions.historyHeight);
    LayoutParams params;
    params.position = Position::Absolute;
    params.hasLeft = true;
    params.left = 8.0f;
    params.hasTop = true;
    params.top = 8.0f + 11.0f * linePitch;
    params.minWidth = 464.0f;
    params.maxWidth = 464.0f;
    params.minHeight = mOptions.historyHeight;
    params.maxHeight = mOptions.historyHeight;
    graph->setLayoutParams(params);
    mGraph = graph;
    mLayer->addChild(mGraph);
  }
  // A dev panel sits on top of the product: without a backing colour the page text underneath shows
  // through between the lines and reads as overlapping glyphs.
  mLayer->setBackgroundColor(Color::MakeRGBA8(14, 16, 20, 236));
  mLayer->setPadding({ 8, 8, 8, 8 });
  mLayer->setGap(2);
  // A dev overlay is a fixed-size panel: the caller that positions it (an absolute child of a Flex)
  // sizes it from this, because a Flex measures a column from its children and the measured text height
  // is shorter than what the lines draw.
  float panelHeight = 11.0f * (lineHeight + 2.0f) + 16.0f;
  if (!mHistory.empty()) panelHeight += mOptions.historyHeight + 2.0f;
  // Wide enough that the longest line (render/submit/gpu/vram) stays on one row; a wrapped line draws
  // a second row inside its one-row box and ghosts over the next line.
  mPanelSize = Size{ 480.0f, panelHeight };
  mLayer->setBounds(Rect::MakeWH(mPanelSize.width, mPanelSize.height));
  mLayer->setNeedsLayout();
  // The file is the authority when it exists; a first run keeps the configured default.
  loadSettings();
  mLayer->setVisible(mEnabled);
}

FrameStatsOverlay::~FrameStatsOverlay() {
  detach();
}

void FrameStatsOverlay::attachTo(Window& window) {
  detach();
  mWindow = &window;
  setStatistics(window.frameStatistics());
  mFrameCallbackId = window.addFrameCallback([this] {
    if (mWindow != nullptr) setStatistics(mWindow->frameStatistics());
  });
}

void FrameStatsOverlay::detach() {
  if (mWindow != nullptr && mFrameCallbackId != 0) {
    mWindow->removeFrameCallback(mFrameCallbackId);
  }
  mWindow = nullptr;
  mFrameCallbackId = 0;
}

void FrameStatsOverlay::setEnabled(const bool enabled) {
  mEnabled = enabled;
  mLayer->setVisible(enabled);
  saveSettings();
}

void FrameStatsOverlay::loadSettings() {
  if (mOptions.settingsPath.empty()) return;
  std::ifstream file(mOptions.settingsPath);
  if (!file) return;
  std::string line;
  while (std::getline(file, line)) {
    if (line.rfind("enabled=", 0) != 0) continue;
    mEnabled = line.substr(8) != "0";
    mLayer->setVisible(mEnabled);
    return;
  }
}

void FrameStatsOverlay::saveSettings() const {
  if (mOptions.settingsPath.empty()) return;
  std::error_code ignored;
  const std::filesystem::path path(mOptions.settingsPath);
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ignored);
  }
  std::ofstream file(path, std::ios::trunc);
  if (!file) return;
  file << "enabled=" << (mEnabled ? 1 : 0) << "\n";
}

std::string FrameStatsOverlay::format(const FrameStatistics& statistics) {
  char buffer[512];
  std::snprintf(buffer, sizeof(buffer),
    "frames %llu  avg %.2f ms  last %.2f ms  present %.2f ms  %s  queue %u  drops %llu  fails %llu"
    "  layers %u (%u visible)  recoveries %u  render %.2f ms  submit %.2f ms  gpu %.2f ms"
    "  vram %.1f MiB  layout %u/%u dirty  dmg %.0f%%",
    static_cast<unsigned long long>(statistics.frames), statistics.averageTotalMilliseconds,
    statistics.lastTotalMilliseconds, statistics.lastPresentMilliseconds, hostName(statistics),
    statistics.queueDepth, static_cast<unsigned long long>(statistics.droppedFrames),
    static_cast<unsigned long long>(statistics.failedFrames), statistics.layers,
    statistics.visibleLayers, statistics.recoveries, statistics.lastRenderMilliseconds,
    statistics.lastExecuteMilliseconds, statistics.lastGpuMilliseconds,
    static_cast<double>(statistics.residentBytes) / (1024.0 * 1024.0),
    statistics.layoutDirtyNodes, statistics.dirtyNodes, statistics.damagedAreaRatio * 100.0);
  // The single-line form is what a product logs; it carries the stage and draw-source totals too, so a
  // log line and the overlay tell the same story.
  std::string text = buffer;
  {
    char extra[192];
    std::snprintf(extra, sizeof(extra), "  record %.2f  fields %.2f  draws image %.2f  path %.2f",
      statistics.renderStages.record, statistics.renderStages.fields,
      statistics.renderStages.image, statistics.renderStages.path);
    text += extra;
  }
  if (statistics.refreshHertz > 0.0) {
    char hz[32];
    std::snprintf(hz, sizeof(hz), " %.0f Hz", statistics.refreshHertz);
    text += hz;
  }
  return text;
}

void FrameStatsOverlay::setStatistics(const FrameStatistics& statistics) {
  char buffer[128];
  std::snprintf(buffer, sizeof(buffer), "frames %llu",
    static_cast<unsigned long long>(statistics.frames));
  mFrames->setText(buffer);
  mAverage->setText(millisecondsLine("avg", statistics.averageTotalMilliseconds));
  mLast->setText(millisecondsLine("last", statistics.lastTotalMilliseconds));

  std::snprintf(buffer, sizeof(buffer), "present %.2f ms  %s",
    statistics.lastPresentMilliseconds, hostName(statistics));
  std::string host = buffer;
  if (statistics.refreshHertz > 0.0) {
    std::snprintf(buffer, sizeof(buffer), " %.0f Hz", statistics.refreshHertz);
    host += buffer;
  }
  mHost->setText(std::move(host));

  std::snprintf(buffer, sizeof(buffer), "render %.2f ms  submit %.2f ms  gpu %.2f ms  vram %.1f MiB",
    statistics.lastRenderMilliseconds, statistics.lastExecuteMilliseconds,
    statistics.lastGpuMilliseconds,
    static_cast<double>(statistics.residentBytes) / (1024.0 * 1024.0));
  mStages->setText(buffer);

  // The stage totals say where a frame goes; the draw-source split says which route inside `record`
  // pays for it, which is the difference between "recording is slow" and "the image route is slow".
  std::snprintf(buffer, sizeof(buffer), "stages fields %.2f  place %.2f  pre %.2f  targets %.2f",
    statistics.renderStages.fields, statistics.renderStages.placeImages,
    statistics.renderStages.precompile, statistics.renderStages.targets);
  mStageFields->setText(buffer);
  std::snprintf(buffer, sizeof(buffer), "stages arena %.2f  upload %.2f  record %.2f  execute %.2f",
    statistics.renderStages.arena, statistics.renderStages.vertexUpload,
    statistics.renderStages.record, statistics.renderStages.execute);
  mStageRecord->setText(buffer);
  std::snprintf(buffer, sizeof(buffer),
    "draws rect %.2f  grad %.2f  image %.2f  path %.2f  field %.2f", statistics.renderStages.rect,
    statistics.renderStages.gradient, statistics.renderStages.image, statistics.renderStages.path,
    statistics.renderStages.field);
  mDrawSources->setText(buffer);

  std::snprintf(buffer, sizeof(buffer), "queue %u  drops %llu  fails %llu  recoveries %u",
    statistics.queueDepth, static_cast<unsigned long long>(statistics.droppedFrames),
    static_cast<unsigned long long>(statistics.failedFrames), statistics.recoveries);
  mPipeline->setText(buffer);
  std::snprintf(buffer, sizeof(buffer), "layers %u (%u visible)", statistics.layers,
    statistics.visibleLayers);
  mContent->setText(buffer);
  if (mProcessTick++ % kProcessSampleInterval == 0) {
    mProcess->setText(formatProcessStatistics(processStatistics()));
  }
  if (!mHistory.empty()) {
    mHistory[mHistoryNext] = statistics.lastTotalMilliseconds;
    mHistoryNext = (mHistoryNext + 1) % mHistory.size();
    mHistoryLast = statistics.lastTotalMilliseconds;
    mHistoryPeak = 1.0;
    for (const double sample : mHistory) {
      mHistoryPeak = std::max(mHistoryPeak, sample);
    }
    if (mGraph != nullptr) mGraph->setNeedsPaint();
  }
  mLayer->setNeedsLayout();
}

uint32_t FrameStatsOverlay::historySampleCount() const noexcept {
  return static_cast<uint32_t>(mHistory.size());
}

double FrameStatsOverlay::historyPeakMilliseconds() const noexcept {
  return mHistoryPeak;
}

double FrameStatsOverlay::historyLastMilliseconds() const noexcept {
  return mHistoryLast;
}

void FrameStatsOverlay::drawHistory(Canvas& canvas, const Rect& bounds) const {
  if (mHistory.empty() || bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f) return;

  graphics::Paint background;
  background.color = Color::MakeRGBA8(0, 0, 0, 160);
  canvas.drawRect(bounds, background);

  // Scaled to the worse of the 60 Hz budget and the window's own peak, so a scene that fits the budget
  // still shows its shape instead of a flat green strip, and a spike is not lost in the noise.
  constexpr double kBudget = 1000.0 / 60.0;
  const double scale = std::max(kBudget, mHistoryPeak);
  const size_t count = mHistory.size();
  const float slot = bounds.getWidth() / static_cast<float>(count);
  const float barWidth = std::max(1.0f, slot - 1.0f);
  for (size_t index = 0; index < count; ++index) {
    const double value = mHistory[(mHistoryNext + index) % count];
    const float height = static_cast<float>(std::min(1.0, value / scale)) * bounds.getHeight();
    const float left = bounds.getLeft() + static_cast<float>(index) * slot;
    const float top = bounds.getBottom() - height;
    graphics::Paint bar;
    bar.color = value <= kBudget          ? Color::MakeRGBA8(80, 200, 120, 255)
      : value <= 2.0 * kBudget           ? Color::MakeRGBA8(230, 190, 80, 255)
                                         : Color::MakeRGBA8(230, 90, 90, 255);
    canvas.drawRect(Rect::MakeLTRB(left, top, left + barWidth, bounds.getBottom()), bar);
  }
}

} // namespace coral::ui
