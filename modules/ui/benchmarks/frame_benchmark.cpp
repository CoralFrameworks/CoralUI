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

/**
 * A frame-budget benchmark for the UI layer.
 *
 * It builds an editor-shaped tree -- a header, a scrollable list of rows, an inspector column and a
 * footer -- and renders it through the real `Window` loop (headless, so no display is required). The
 * per-frame breakdown comes from `Window::lastFrameTimings`, which is the same data an application
 * overlay would show.
 *
 * Two modes matter:
 *   - steady state (default): the tree is unchanged, so only draw + submit are measured. This is the
 *     number the "do we need a retained display list?" decision rests on.
 *   - `--relayout`: marks the tree dirty before every frame, so layout is measured too. This is what
 *     an edit-time frame costs.
 *
 * Usage:
 *   frame_benchmark [--width W] [--height H] [--rows N] [--frames N] [--warmup N]
 *                   [--scale S] [--relayout] [--out FILE]
 */

#include <ui/Button.h>
#include <ui/ColorLayer.h>
#include <ui/Flex.h>
#include <ui/Layer.h>
#include <ui/ProcessStats.h>
#include <ui/ProgressBar.h>
#include <ui/ScrollView.h>
#include <ui/TextLayer.h>
#include <ui/TextRendererStats.h>
#include <ui/Theme.h>
#include <ui/Window.h>
#include <ui/WindowBackend.h>

#include <graphics/Color.h>
#include <graphics/Font.h>
#include <graphics/Paint.h>
#include <graphics/Path.h>
#include <graphics/Typeface.h>

#include <backend/Device.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

namespace {

using coral::ui::Align;
using coral::ui::Axis;
using coral::ui::ColorLayer;
using coral::ui::Flex;
using coral::ui::Layer;
using coral::ui::Rect;
using coral::ui::ScrollView;
using coral::ui::TextLayer;

/**
 * The build type this binary was compiled as.
 *
 * It is printed in the banner and enforced by `--require-release` because the same scene costs 15-20x
 * more in a debug build: the architecture doc and the R3/R4 project were both written against debug
 * numbers, which read as "scrolling costs 26-34 ms" when release costs 1.8 ms for the same tree. A
 * performance gate that cannot tell which build it measured is a gate that can pass on the wrong one.
 */
#if defined(NDEBUG)
constexpr const char* kBuildType = "release";
#else
constexpr const char* kBuildType = "debug";
#endif

/** A panel whose own paint is expensive: many rects, and larger than the default recording cap. */
class HeavyPanel final : public coral::ui::Layer {
 public:
  HeavyPanel(const int index, const int rects) : mIndex(index), mRects(rects) {}

 protected:
  void drawSelf(coral::ui::RenderContext& context) override {
    auto& canvas = context.canvas();
    for (int rect = 0; rect < mRects; ++rect) {
      graphics::Paint paint;
      paint.color = graphics::Color::MakeRGBA8(
        static_cast<uint8_t>(24 + (rect * 7 + mIndex * 29) % 200),
        static_cast<uint8_t>(48 + (rect * 3) % 120),
        static_cast<uint8_t>(72 + (rect * 5) % 160), 255);
      const float x = bounds().getLeft() + 8.0f + static_cast<float>(rect % 16) * 14.0f;
      const float y = bounds().getTop() + 8.0f + static_cast<float>((rect / 16) % 24) * 12.0f;
      canvas.drawRect(Rect::MakeLTRB(x, y, x + 12.0f, y + 10.0f), paint);
    }
  }

 private:
  int mIndex = 0;
  int mRects = 0;
};

enum class ClipMode {
  None,
  Path,
  Rounded,
  Group,
};

const char* clipModeName(const ClipMode mode) {
  switch (mode) {
    case ClipMode::None: return "none";
    case ClipMode::Path: return "path";
    case ClipMode::Rounded: return "rounded";
    case ClipMode::Group: return "group";
  }
  return "none";
}

/** An octagon inset in @p bounds: enough corners that the clip cannot pass for its rectangle. */
graphics::Path panelClipPath(const Rect& bounds) {
  const float inset = 10.0f;
  const float cut = 26.0f;
  const float left = bounds.getLeft() + inset;
  const float top = bounds.getTop() + inset;
  const float right = bounds.getRight() - inset;
  const float bottom = bounds.getBottom() - inset;
  graphics::Path path;
  path.moveTo({ left + cut, top });
  path.lineTo({ right - cut, top });
  path.lineTo({ right, top + cut });
  path.lineTo({ right, bottom - cut });
  path.lineTo({ right - cut, bottom });
  path.lineTo({ left + cut, bottom });
  path.lineTo({ left, bottom - cut });
  path.lineTo({ left, top + cut });
  path.close();
  return path;
}

/**
 * One preview tile: a background, bars and a label, drawn inside a shaped clip.
 *
 * `ClipMode::Path` goes through `Canvas::clipPath` (the per-command coverage mask the renderer
 * rewrites into a masked layer). `ClipMode::Rounded` uses `Layer::setClipRadius`, which spends a
 * target-sized CPU mask and one layer-mask composite. `ClipMode::Group` is the control for R1 step
 * one: a plain opacity group with no mask, so its target can be sized to the panel.
 * `None` is the no-layer control.
 */
class ClipPanel final : public Layer {
 public:
  ClipPanel(ClipMode mode, std::shared_ptr<const graphics::Font> font, int index, bool rounded,
    bool text)
      : mMode(mode), mFont(std::move(font)), mIndex(index), mText(text) {
    if (rounded) {
      setClip(true);
      setClipRadius(20.0f);
    } else if (mode == ClipMode::Group) {
      setOpacity(0.99f);
    }
  }

  void drawSelf(coral::ui::RenderContext& context) override {
    auto& canvas = context.canvas();
    const Rect box = bounds();
    const bool pathClip = mMode == ClipMode::Path;
    if (pathClip) {
      canvas.save();
      canvas.clipPath(panelClipPath(box));
    }

    graphics::Paint background;
    background.color = graphics::Color::MakeRGBA8(static_cast<uint8_t>(28 + (mIndex % 5) * 8),
      static_cast<uint8_t>(32 + (mIndex % 3) * 10), 46, 255);
    background.style = graphics::Paint::Style::kFill;
    background.antiAlias = false;
    canvas.drawRect(box, background);

    for (int bar = 0; bar < 5; ++bar) {
      const float height = 6.0f + static_cast<float>(bar) * 4.0f;
      const float y = box.getTop() + 18.0f + static_cast<float>(bar) * 26.0f;
      graphics::Paint paint;
      paint.color = graphics::Color::MakeRGBA8(static_cast<uint8_t>(70 + bar * 30),
        static_cast<uint8_t>(140 - bar * 12), 210, 255);
      paint.style = graphics::Paint::Style::kFill;
      paint.antiAlias = false;
      canvas.drawRect(Rect::MakeXYWH(box.getLeft() + 16.0f, y,
        std::max(0.0f, box.getWidth() - 32.0f - static_cast<float>(bar) * 12.0f), height), paint);
    }

    if (mFont != nullptr && mText) {
      graphics::Paint text;
      text.color = graphics::Color::MakeRGBA8(228, 233, 242, 255);
      text.style = graphics::Paint::Style::kFill;
      context.drawText("Preview " + std::to_string(mIndex), box.getLeft() + 18.0f,
        box.getTop() + 32.0f, *mFont, text);
    }

    if (pathClip) {
      canvas.restore();
    }

    // A border outside the clip, so the tile still reads as a card and the clip's edge is visible. It
    // is part of the text-free control too: `--no-text` is the R1 rect/image scene, and a stroked
    // border is neither.
    if (mText) {
      graphics::Paint border;
      border.color = graphics::Color::MakeRGBA8(86, 96, 118, 255);
      border.style = graphics::Paint::Style::kStroke;
      border.strokeWidth = 1.0f;
      canvas.drawRectStroke(box, border);
    }
  }

 private:
  ClipMode mMode;
  std::shared_ptr<const graphics::Font> mFont;
  int mIndex = 0;
  bool mText = true;
};

/** The first scroll view in the tree, which the scroll scenario drives. */
ScrollView* firstScrollView(Layer& layer) {
  if (auto* view = dynamic_cast<ScrollView*>(&layer); view != nullptr) return view;
  for (const std::shared_ptr<Layer>& child: layer.children()) {
    if (child == nullptr) continue;
    if (ScrollView* found = firstScrollView(*child); found != nullptr) return found;
  }
  return nullptr;
}

std::size_t countNodes(const Layer& layer) {
  std::size_t count = 1;
  for (const auto& child: layer.children()) {
    count += countNodes(*child);
  }
  return count;
}

double percentile(const std::vector<double>& sorted, const double fraction) {
  if (sorted.empty()) return 0.0;
  const std::size_t index = std::min(sorted.size() - 1,
    static_cast<std::size_t>(fraction * static_cast<double>(sorted.size() - 1)));
  return sorted[index];
}

void report(const char* name, std::vector<double> samples) {
  std::sort(samples.begin(), samples.end());
  const double mean = std::accumulate(samples.begin(), samples.end(), 0.0) /
                      static_cast<double>(samples.size());
  std::printf("  %-8s p50=%6.3f  p95=%6.3f  mean=%6.3f ms\n", name, percentile(samples, 0.50),
    percentile(samples, 0.95), mean);
}

std::shared_ptr<Layer> makeRow(const std::shared_ptr<const graphics::Font>& font, const int index) {
  auto row = std::make_shared<Flex>(Axis::Row);
  row->setGap(8.0f);
  row->setCrossAlign(Align::Center);
  auto label = [&](const std::string& text, const float weight) {
    auto layer = std::make_shared<TextLayer>(text, font,
      graphics::Color::MakeRGBA8(226, 232, 240, 255));
    coral::ui::LayoutParams params;
    params.grow = weight;
    params.basis = 0.0f;
    layer->setLayoutParams(params);
    return layer;
  };
  auto name = label("Clip " + std::to_string(index), 2.0f);
  name->setMaxLines(1);
  row->addChild(name);
  row->addChild(label("00:0" + std::to_string(index % 10) + ":12", 1.0f));
  row->addChild(label("04:2" + std::to_string(index % 10), 1.0f));
  auto bar = std::make_shared<coral::ui::ProgressBar>(font, 0.25 + 0.5 * ((index % 7) / 7.0));
  coral::ui::LayoutParams barParams;
  barParams.grow = 1.0f;
  barParams.basis = 0.0f;
  bar->setLayoutParams(barParams);
  row->addChild(bar);

  auto swatch = std::make_shared<ColorLayer>(
    graphics::Color::MakeRGBA8(static_cast<uint8_t>(40 + (index % 5) * 12), 90, 160, 255));
  swatch->setBounds(Rect::MakeWH(8.0f, 16.0f));
  row->addChild(swatch);
  return row;
}

/**
 * A virtual list of editor rows: it keeps only the rows near the viewport and recycles the rest.
 *
 * This is what `ScrollView::VirtualContent` is for and what the scene measures with `--virtual N`:
 * 10000 rows must cost what a viewport costs, in nodes, layout and drawing, because only the rows the
 * viewport can reach exist.
 */
class VirtualRows final : public coral::ui::ScrollView::VirtualContent {
 public:
  VirtualRows(std::shared_ptr<const graphics::Font> font, const int rows, const float rowHeight)
    : mFont(std::move(font)), mRows(rows), mRowHeight(rowHeight) {}

  [[nodiscard]] coral::ui::Size contentSize(const coral::ui::Size& viewport) override {
    return coral::ui::Size{ std::max(viewport.width, 480.0f),
      static_cast<float>(mRows) * mRowHeight };
  }

  void realize(Layer& host, const float top, const float bottom) override {
    const int first = std::max(0, static_cast<int>(top / mRowHeight));
    const int last = std::min(mRows - 1, static_cast<int>((bottom - 1.0f) / mRowHeight));
    const std::size_t needed = static_cast<std::size_t>(std::max(0, last - first + 1));
    while (mPool.size() < needed) {
      std::shared_ptr<Layer> row = makeRow(mFont, static_cast<int>(mPool.size()));
      host.addChild(row);
      mPool.push_back(row);
    }
    for (std::size_t index = 0; index < mPool.size(); ++index) {
      const bool used = index < needed;
      mPool[index]->setVisible(used);
      if (!used) continue;
      const int rowIndex = first + static_cast<int>(index);
      mPool[index]->setBounds(Rect::MakeXYWH(host.bounds().getLeft(),
        host.bounds().getTop() + static_cast<float>(rowIndex) * mRowHeight,
        std::max(host.bounds().getWidth(), 480.0f), mRowHeight));
    }
  }

  void detach(Layer& host) override {
    for (const std::shared_ptr<Layer>& row: mPool) {
      host.removeChild(row);
    }
    mPool.clear();
  }

  [[nodiscard]] std::size_t poolSize() const noexcept {
    return mPool.size();
  }

 private:
  std::shared_ptr<const graphics::Font> mFont;
  int mRows = 0;
  float mRowHeight = 32.0f;
  std::vector<std::shared_ptr<Layer>> mPool;
};

} // namespace

int main(int argc, char** argv) {
  int width = 1920;
  int height = 1080;
  int rows = 240;
  int virtualRows = 0;
  bool traceStages = false;
  int clips = 0;
  int frames = 120;
  int warmup = 5;
  int windowX = -1;
  int windowY = -1;
  bool hdrOutput = false;
  int holdMs = 0;
  int cacheScene = 0;
  int cacheLimitPx = 512;
  int cacheBudgetMiB = 64;
  int cacheHeavy = 0;
  bool dirtyChild = false;
  bool scroll = false;
  float scrollStep = 4.0f;
  bool boundaries = false;
  int cacheTiles = 4;
  float scale = 1.0f;
  double budgetMs = 0.0;
  /** Ceiling for process-memory growth across the measured frames; 0 disables the check. */
  double budgetGrowMiB = 0.0;
  /** Ceiling for live device memory (textures and buffers); 0 disables the check. */
  double budgetVramMiB = 0.0;
  /** Ceiling for the growth in open handles/file descriptors across the run; 0 disables the check. */
  int budgetHandleGrowth = 0;
  float panelFraction = 1.0f;
  bool panelText = true;
  enum class BudgetMetric {
    P50,
    Mean,
    P95,
  };
  BudgetMetric budgetMetric = BudgetMetric::P95;
  bool relayout = false;
  bool dirty = false;
  bool native = false;
  bool allowNoGpu = false;
  bool requireRelease = false;
  bool raster = false;
  ClipMode clipMode = ClipMode::Path;
  coral::ui::RendererApi preferred = coral::ui::RendererApi::Auto;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    const bool hasValue = index + 1 < argc;
    if (argument == "--width" && hasValue) width = std::max(16, std::atoi(argv[++index]));
    else if (argument == "--height" && hasValue) height = std::max(16, std::atoi(argv[++index]));
    else if (argument == "--rows" && hasValue) rows = std::max(1, std::atoi(argv[++index]));
  else if (argument == "--virtual" && hasValue) virtualRows = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--clips" && hasValue) clips = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--cache-scene" && hasValue)
      cacheScene = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--cache-limit" && hasValue)
      cacheLimitPx = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--cache-budget" && hasValue)
      cacheBudgetMiB = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--cache-heavy" && hasValue)
      cacheHeavy = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--cache-tiles" && hasValue)
      cacheTiles = std::max(1, std::min(8, std::atoi(argv[++index])));
    else if (argument == "--frames" && hasValue) frames = std::max(1, std::atoi(argv[++index]));
    else if (argument == "--warmup" && hasValue) warmup = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--scale" && hasValue)
      scale = std::max(0.25f, static_cast<float>(std::atof(argv[++index])));
    else if (argument == "--budget-ms" && hasValue) budgetMs = std::atof(argv[++index]);
    else if (argument == "--budget-grow-mib" && hasValue)
      budgetGrowMiB = std::atof(argv[++index]);
    else if (argument == "--budget-vram-mib" && hasValue)
      budgetVramMiB = std::atof(argv[++index]);
    else if (argument == "--budget-handle-growth" && hasValue)
      budgetHandleGrowth = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--panel-fraction" && hasValue)
      panelFraction = std::clamp(static_cast<float>(std::atof(argv[++index])), 0.1f, 1.0f);
    else if (argument == "--no-text") panelText = false;
    else if (argument == "--budget-metric" && hasValue) {
      const std::string name = argv[++index];
      if (name == "p50") budgetMetric = BudgetMetric::P50;
      else if (name == "mean") budgetMetric = BudgetMetric::Mean;
      else if (name == "p95") budgetMetric = BudgetMetric::P95;
    }
    else if (argument == "--window-x" && hasValue) windowX = std::atoi(argv[++index]);
    else if (argument == "--window-y" && hasValue) windowY = std::atoi(argv[++index]);
    else if (argument == "--hdr") hdrOutput = true;
    else if (argument == "--hold-ms" && hasValue) holdMs = std::max(0, std::atoi(argv[++index]));
    else if (argument == "--allow-no-gpu") allowNoGpu = true;
    else if (argument == "--trace-stages") traceStages = true;
    else if (argument == "--require-release") requireRelease = true;
    else if (argument == "--raster") raster = true;
    else if (argument == "--clip-mode" && hasValue) {
      const std::string name = argv[++index];
      if (name == "none") clipMode = ClipMode::None;
      else if (name == "path") clipMode = ClipMode::Path;
      else if (name == "rounded") clipMode = ClipMode::Rounded;
      else if (name == "group") clipMode = ClipMode::Group;
    }
    else if (argument == "--backend" && hasValue) {
      const std::string name = argv[++index];
      if (name == "gl" || name == "opengl") preferred = coral::ui::RendererApi::OpenGL;
      else if (name == "vk" || name == "vulkan") preferred = coral::ui::RendererApi::Vulkan;
      else if (name == "raster" || name == "cpu") raster = true;
    }
    else if (argument == "--relayout") relayout = true;
    else if (argument == "--dirty") dirty = true;
    else if (argument == "--dirty-child") dirtyChild = true;
    else if (argument == "--scroll") scroll = true;
    else if (argument == "--scroll-step" && hasValue) scrollStep = static_cast<float>(std::atof(argv[++index]));
    else if (argument == "--boundaries") boundaries = true;
    else if (argument == "--native") native = true;
    else if (argument == "--help") {
      std::printf("usage: frame_benchmark [--width W] [--height H] [--rows N] [--clips N] "
                  "[--clip-mode none|path|rounded|group] [--cache-scene C] [--cache-limit PX] [--cache-budget MIB] [--cache-tiles N] "
                  "[--panel-fraction F] [--no-text] [--frames N] [--warmup N] [--virtual N] "
                  "[--scale S] [--backend gl|vk|raster] [--raster] [--relayout] [--dirty] [--native] [--budget-ms MS] "
                  "[--window-x X] [--window-y Y] [--hdr] [--hold-ms N] [--budget-metric p50|mean|p95] [--budget-grow-mib MiB] [--budget-vram-mib MiB] "
                  "[--budget-handle-growth N] [--allow-no-gpu] [--require-release]\n");
      return 0;
    }
  }

  // Gates run this before touching the device: a debug binary measures a build 15-20x slower on the
  // same tree, so a threshold tuned here would be meaningless there. Reported as its own exit code so
  // a caller can tell "wrong build" from "over budget".
  if (requireRelease && std::strcmp(kBuildType, "release") != 0) {
    std::fprintf(stderr,
      "frame_benchmark: this run requires a release build (build=%s); configure with "
      "-DCMAKE_BUILD_TYPE=Release\n", kBuildType);
    return 6;
  }

  const auto typeface = graphics::Typeface::fromFile(std::string(CORAL_ASSETS_DIR) +
    "/fonts/NotoSansSC-Regular.otf");
  if (typeface == nullptr) {
    std::fprintf(stderr, "the UI font could not be loaded\n");
    return 1;
  }
  const auto bodyFont = graphics::Font::create(typeface, 14.0f);
  const auto titleFont = graphics::Font::create(typeface, 20.0f);

  coral::ui::Window::Options options;
  options.headless = !native || raster;
  options.rendererBackend = raster ? coral::ui::RendererBackend::Raster
                                   : coral::ui::RendererBackend::Auto;
  options.width = width;
  options.height = height;
  options.uiScale = scale;
  options.rendererApi = preferred;
  options.clearColor = coral::ui::Theme::dark().background;
  if (windowX >= 0 || windowY >= 0) {
    options.position = coral::ui::Point{ static_cast<float>(windowX), static_cast<float>(windowY) };
  }
  options.hdrOutput = hdrOutput;
  auto window = coral::ui::Window::create(options);
  if (window == nullptr) {
    if (allowNoGpu) {
      std::printf("frame_benchmark: SKIP (no GPU device is available)\n");
      return 0;
    }
    std::fprintf(stderr, "no GPU device is available for the benchmark\n");
    return 1;
  }

  // ---- The tree: header, body (list + inspector), footer -------------------------------
  auto buildEditor = [&]() -> std::shared_ptr<Layer> {
  auto root = std::make_shared<Flex>(Axis::Column);
  root->setPadding({ 12.0f, 12.0f, 12.0f, 12.0f });
  root->setGap(8.0f);
  // Stretch, or the column gives every child its measured width: a Row of grow/basis children then
  // measures to the gaps alone and the whole scene collapses to a twelve-pixel-wide viewport.
  root->setCrossAlign(Align::Stretch);

  auto header = std::make_shared<Flex>(Axis::Row);
  header->setGap(12.0f);
  header->setCrossAlign(Align::Center);
  auto title = std::make_shared<TextLayer>("Coral Editor", titleFont,
    graphics::Color::MakeRGBA8(226, 232, 240, 255));
  header->addChild(title);
  auto status = std::make_shared<TextLayer>("Timeline: " + std::to_string(rows) + " clips",
    bodyFont, graphics::Color::MakeRGBA8(148, 163, 184, 255));
  coral::ui::LayoutParams statusParams;
  statusParams.grow = 1.0f;
  status->setLayoutParams(statusParams);
  header->addChild(status);
  header->addChild(std::make_shared<coral::ui::Button>("Open", bodyFont));
  header->addChild(std::make_shared<coral::ui::Button>("Save", bodyFont));
  root->addChild(header);

  auto body = std::make_shared<Flex>(Axis::Row);
  body->setGap(12.0f);
  body->setCrossAlign(Align::Stretch);
  coral::ui::LayoutParams bodyParams;
  bodyParams.grow = 1.0f;
  body->setLayoutParams(bodyParams);

  auto listColumn = std::make_shared<Flex>(Axis::Column);
  listColumn->setGap(4.0f);
  coral::ui::LayoutParams listParams;
  listParams.grow = 1.0f;
  listParams.basis = 0.0f;
  listColumn->setLayoutParams(listParams);
  for (int index = 0; index < rows; ++index) {
    listColumn->addChild(makeRow(bodyFont, index));
  }

  auto scroll = std::make_shared<ScrollView>();
  scroll->setContent(listColumn);
  coral::ui::LayoutParams scrollParams;
  scrollParams.grow = 3.0f;
  scrollParams.basis = 0.0f;
  scroll->setLayoutParams(scrollParams);
  body->addChild(scroll);

  auto inspector = std::make_shared<Flex>(Axis::Column);
  inspector->setGap(6.0f);
  coral::ui::LayoutParams inspectorParams;
  inspectorParams.grow = 1.0f;
  inspectorParams.basis = 0.0f;
  inspector->setLayoutParams(inspectorParams);
  for (int index = 0; index < 12; ++index) {
    auto field = std::make_shared<TextLayer>("Property " + std::to_string(index) + ": 0.50",
      bodyFont, graphics::Color::MakeRGBA8(226, 232, 240, 255));
    field->setMaxLines(1);
    inspector->addChild(field);
    auto bar = std::make_shared<coral::ui::ProgressBar>(bodyFont, (index % 11) / 10.0);
    coral::ui::LayoutParams barParams;
    barParams.minHeight = 6.0f;
    bar->setLayoutParams(barParams);
    inspector->addChild(bar);
  }
  body->addChild(inspector);
  root->addChild(body);

  auto footer = std::make_shared<Flex>(Axis::Row);
  footer->setGap(8.0f);
  footer->setCrossAlign(Align::Center);
  for (const char* label: { "Play", "Stop", "Prev", "Next" }) {
    footer->addChild(std::make_shared<coral::ui::Button>(label, bodyFont));
  }
  footer->addChild(std::make_shared<TextLayer>("00:00:00.000 / 00:03:20.000", bodyFont,
    graphics::Color::MakeRGBA8(148, 163, 184, 255)));
  root->addChild(footer);
  return root;
  };

  // ---- Or the clip-heavy scene: N preview tiles, all shaped-clipped ---------------------
  coral::ui::Layer::setRecordingCacheAreaLimit(static_cast<float>(cacheLimitPx) * static_cast<float>(cacheLimitPx));
  coral::ui::Layer::setRecordingCacheBudgetBytes(
    static_cast<std::size_t>(cacheBudgetMiB) * 1024u * 1024u);

  // ---- Or the recording-cache scene: CxC panels, each a grid of fixed-size tiles ---------
  //
  // The point of this scene is the *same* geometry measured on both sides of `Layer`'s recording
  // cache: at the default limit (512 px) a panel of this size is recorded and replayed, while
  // `--cache-limit 64` pushes every panel over the limit and makes the frame redraw the tiles. That is
  // the "replay vs re-record" comparison the cache redesign needs, without a rebuild between runs.
  const auto buildCacheScene = [&]() -> std::shared_ptr<Layer> {
    auto scene = std::make_shared<Layer>();
    const float logicalWidth = static_cast<float>(width) / scale;
    const float logicalHeight = static_cast<float>(height) / scale;
    scene->setBounds(Rect::MakeWH(logicalWidth, logicalHeight));
    const int grid = std::max(1, cacheScene);
    const float cellWidth = logicalWidth / grid;
    const float cellHeight = logicalHeight / grid;
    const float gap = 6.0f;
    int panelIndex = 0;
    for (int row = 0; row < grid; ++row) {
      for (int column = 0; column < grid; ++column) {
        const Rect box = Rect::MakeLTRB(column * cellWidth + gap, row * cellHeight + gap,
          (column + 1) * cellWidth - gap, (row + 1) * cellHeight - gap);
        auto panel = std::make_shared<Layer>();
        panel->setBounds(box);
        const int tiles = std::max(1, cacheTiles);
        const float tileWidth = (box.getWidth() - gap * 2.0f) / tiles;
        const float tileHeight = (box.getHeight() - gap * 2.0f) / tiles;
        for (int tileRow = 0; tileRow < tiles; ++tileRow) {
          for (int tileColumn = 0; tileColumn < tiles; ++tileColumn) {
            const float left = box.getLeft() + gap + tileColumn * tileWidth;
            const float top = box.getTop() + gap + tileRow * tileHeight;
            // A text tile rather than a solid one: recording a subtree is what the cache saves, and a
            // solid rectangle records one command, which is too little for the saving to show. A line of
            // text records one command per glyph, so a panel of text tiles is the "expensive to record"
            // child the cheap-tile scene was missing.
            const std::string label = "row " + std::to_string(panelIndex) + " " +
              std::to_string(tileRow * tiles + tileColumn) + ": the quick brown fox jumps over 0123456789";
            const uint8_t level = static_cast<uint8_t>(100 + ((panelIndex + tileRow * tiles + tileColumn) * 7) % 120);
            auto tile = std::make_shared<TextLayer>(label, bodyFont,
              graphics::Color::MakeRGBA8(level, static_cast<uint8_t>(255 - level / 2), 160, 255));
            tile->setMaxLines(1);
            tile->setBounds(Rect::MakeLTRB(left, top, left + tileWidth - gap * 0.5f,
              top + tileHeight - gap * 0.5f));
            panel->addChild(tile);
          }
        }
        scene->addChild(panel);
        ++panelIndex;
      }
    }
    return scene;
  };

  // ---- Or the heavy-paint scene: N large panels whose own paint is expensive ------------
  //
  // Each panel is larger than the recording area cap, so with the default limit the panel re-records
  // its own paint every frame; with `--cache-limit 0` the same panel fits the budget and is recorded
  // once. That is the R5 comparison: a layer whose *own* paint is the cost, not its children.
  const auto buildHeavyScene = [&]() -> std::shared_ptr<Layer> {
    auto scene = std::make_shared<Layer>();
    const float logicalWidth = static_cast<float>(width) / scale;
    const float logicalHeight = static_cast<float>(height) / scale;
    scene->setBounds(Rect::MakeWH(logicalWidth, logicalHeight));
    const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(
      static_cast<float>(cacheHeavy) * logicalWidth / std::max(1.0f, logicalHeight)))));
    const int rows = (cacheHeavy + columns - 1) / columns;
    const float cellWidth = logicalWidth / static_cast<float>(columns);
    const float cellHeight = logicalHeight / static_cast<float>(rows);
    for (int index = 0; index < cacheHeavy; ++index) {
      const int column = index % columns;
      const int row = index / columns;
      auto panel = std::make_shared<HeavyPanel>(index, 320);
      const float left = static_cast<float>(column) * cellWidth + 8.0f;
      const float top = static_cast<float>(row) * cellHeight + 8.0f;
      panel->setBounds(Rect::MakeLTRB(left, top, left + cellWidth - 16.0f, top + cellHeight - 16.0f));
      scene->addChild(panel);
    }
    return scene;
  };

  std::shared_ptr<Layer> root;
  if (cacheHeavy > 0) {
    root = buildHeavyScene();
  } else if (cacheScene > 0) {
    root = buildCacheScene();
  } else if (clips > 0) {
    root = std::make_shared<Layer>();
    const float logicalWidth = static_cast<float>(width) / scale;
    const float logicalHeight = static_cast<float>(height) / scale;
    root->setBounds(Rect::MakeWH(logicalWidth, logicalHeight));

    auto caption = std::make_shared<TextLayer>(
      "Clip panels: " + std::to_string(clips) + " (" + clipModeName(clipMode) + ")", titleFont,
      graphics::Color::MakeRGBA8(226, 232, 240, 255));
    caption->setBounds(Rect::MakeLTRB(16.0f, 10.0f, logicalWidth - 16.0f, 34.0f));
    root->addChild(caption);

    const float padding = 10.0f;
    const float top = 42.0f;
    const float aspect = logicalWidth / std::max(1.0f, logicalHeight);
    int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(clips * aspect))));
    columns = std::min(columns, clips);
    const int gridRows = (clips + columns - 1) / columns;
    const float cellWidth = (logicalWidth - padding * (columns + 1)) / columns;
    const float cellHeight = (logicalHeight - top - padding * (gridRows + 1)) / gridRows;
    const float panelWidth = std::max(1.0f, std::round(cellWidth * panelFraction));
    const float panelHeight = std::max(1.0f, std::round(cellHeight * panelFraction));
    for (int index = 0; index < clips; ++index) {
      const int column = index % columns;
      const int row = index / columns;
      const float cellLeft = padding + static_cast<float>(column) * (cellWidth + padding);
      const float cellTop = top + padding + static_cast<float>(row) * (cellHeight + padding);
      const float left = std::round(cellLeft + (cellWidth - panelWidth) * 0.5f);
      const float topInCell = std::round(cellTop + (cellHeight - panelHeight) * 0.5f);
      const Rect box = Rect::MakeLTRB(left, topInCell, left + panelWidth, topInCell + panelHeight);
      auto panel = std::make_shared<ClipPanel>(clipMode, bodyFont, index,
        clipMode == ClipMode::Rounded, panelText);
      panel->setBounds(box);
      root->addChild(panel);
    }
  } else {
    root = buildEditor();
  }

  ScrollView* scrolledView = scroll && root != nullptr ? firstScrollView(*root) : nullptr;
  std::shared_ptr<VirtualRows> virtualProvider;
  if (virtualRows > 0 && scrolledView != nullptr) {
    // Replace the materialized rows with a lazy list: the scene keeps its shape (text rows in a scrolled
    // page) but only the rows the viewport can reach exist.
    virtualProvider = std::make_shared<VirtualRows>(bodyFont, virtualRows, 32.0f);
    scrolledView->setVirtualContent(virtualProvider);
    scrolledView->setRepaintBoundary(true);
  }
  // `--boundaries` marks the scrolled viewport as the repaint boundary. That is the shape the plan path
  // is for: the viewport lends the one or two band lists it currently shows, on top of the frame's single
  // content list. Marking every panel instead measures per-list submission overhead, not the viewport.
  if (boundaries && scrolledView != nullptr) {
    scrolledView->setRepaintBoundary(true);
  }

  window->setRoot(root);

  // Warm up: the first frame builds pipelines and text blobs, and is not representative.
  for (int frame = 0; frame < warmup; ++frame) {
    if (!window->renderFrame()) {
      std::fprintf(stderr, "a warm-up frame was refused: %s\n", window->diagnostics().c_str());
      return 1;
    }
  }

  const auto warmupGrowStart = coral::ui::processStatistics();
  std::vector<double> update;
  std::vector<double> layout;
  std::vector<double> draw;
  std::vector<double> paint;
  std::vector<double> present;
  std::vector<double> total;
  update.reserve(static_cast<std::size_t>(frames));
  layout.reserve(static_cast<std::size_t>(frames));
  draw.reserve(static_cast<std::size_t>(frames));
  paint.reserve(static_cast<std::size_t>(frames));
  present.reserve(static_cast<std::size_t>(frames));
  total.reserve(static_cast<std::size_t>(frames));
  double dirtySum = 0.0;
  double layoutDirtySum = 0.0;
  double damageSum = 0.0;
  double repaintedSum = 0.0;
  double frameListSum = 0.0;
  double planItemSum = 0.0;
  double presentItemSum = 0.0;
  double compositeItemSum = 0.0;
  double lentByteSum = 0.0;
  double culledSum = 0.0;
  double commandBufferSum = 0.0;
  double stageFieldsSum = 0.0;
  double stagePrecompileSum = 0.0;
  double stageArenaSum = 0.0;
  double stageVertexUploadSum = 0.0;
  double stageRecordSum = 0.0;
  double stageExecuteSum = 0.0;
  double stageTotalSum = 0.0;
  double drawCallSum = 0.0;
  double batchSum = 0.0;
  double stageRectSum = 0.0;
  double stageGradientSum = 0.0;
  double stageImageSum = 0.0;
  double stagePathSum = 0.0;
  double stageFieldSum = 0.0;
  double stageCompositeSum = 0.0;
  std::uint32_t repaintedMax = 0;
  std::uint32_t dirtyMax = 0;
  std::uint32_t layoutDirtyMax = 0;
  // The text counters are cumulative and the warm-up already drew text, so the measured window starts
  // here. `runs/frame` is the number that says whether a frame rebuilt its text: a static tree rebuilds
  // none, and a scene whose runs no longer fit the cache rebuilds them all every frame.
  coral::ui::resetTextRendererStats();

  for (int frame = 0; frame < frames; ++frame) {
    if (relayout) {
      // Forces the next frame through measure/layout, which is the edit-time path.
      root->setNeedsLayout();
    }
    if (dirty) {
      // Forces the recording path even when the tree is static: without this a measured frame reuses
      // the retained recording, which is the right product behaviour but not what a clip benchmark
      // is trying to time.
      root->setNeedsPaint();
    }
    if (scrolledView != nullptr) {
      const float step = static_cast<float>(frame % 64) * scrollStep;
      scrolledView->setScrollOffset(0.0f, step);
    }
    if (dirtyChild && root != nullptr && !root->children().empty()) {
      // Dirties one child instead of the root: the root recording should patch that child segment and
      // keep everything else, which is the property-level invalidation path.
      root->children().front()->setNeedsPaint();
    }
    if (!window->renderFrame()) {
      std::fprintf(stderr, "a measured frame was refused: %s\n", window->diagnostics().c_str());
      return 1;
    }
    const coral::ui::FrameTimings timings = window->lastFrameTimings();
    update.push_back(timings.update);
    layout.push_back(timings.layout);
    draw.push_back(timings.draw);
    paint.push_back(timings.paint);
    present.push_back(timings.present);
    total.push_back(timings.total);
    // Retained-mode counters: how much the frame actually had to redo, independent of its duration.
    const coral::ui::FrameStatistics counters = window->frameStatistics();
    dirtySum += counters.dirtyNodes;
    layoutDirtySum += counters.layoutDirtyNodes;
    repaintedSum += static_cast<double>(counters.repaintedBoundaries);
    frameListSum += static_cast<double>(counters.frameListBytes);
    planItemSum += static_cast<double>(counters.framePlanItems);
    compositeItemSum += static_cast<double>(counters.compositeItems);
    presentItemSum += static_cast<double>(counters.presentItems);
    lentByteSum += static_cast<double>(counters.lentListBytes);
    culledSum += static_cast<double>(counters.culledLayers);
    commandBufferSum += static_cast<double>(counters.commandBuffers);
    stageFieldsSum += counters.renderStages.fields;
    stagePrecompileSum += counters.renderStages.precompile;
    stageArenaSum += counters.renderStages.arena;
    stageVertexUploadSum += counters.renderStages.vertexUpload;
    stageRecordSum += counters.renderStages.record;
    stageExecuteSum += counters.renderStages.execute;
    stageTotalSum += counters.renderStages.total;
    drawCallSum += static_cast<double>(counters.drawCalls);
    batchSum += static_cast<double>(counters.batches);
    stageRectSum += counters.renderStages.rect;
    stageGradientSum += counters.renderStages.gradient;
    stageImageSum += counters.renderStages.image;
    stagePathSum += counters.renderStages.path;
    stageFieldSum += counters.renderStages.field;
    stageCompositeSum += counters.renderStages.composite;
    repaintedMax = std::max(repaintedMax, counters.repaintedBoundaries);
    damageSum += counters.damagedAreaRatio;
    dirtyMax = std::max(dirtyMax, counters.dirtyNodes);
    layoutDirtyMax = std::max(layoutDirtyMax, counters.layoutDirtyNodes);
  }

  const coral::ui::TextRendererStats textStats = coral::ui::textRendererStats();
  const double textFrames = static_cast<double>(std::max(1, frames));

  const std::size_t nodes = countNodes(*root);
  std::sort(total.begin(), total.end());
  const backend::Device* benchmarkDevice = coral::ui::detail::deviceOf(*window);
  if (benchmarkDevice != nullptr) {
    const backend::DeviceInfo& info = benchmarkDevice->getInfo();
    std::printf("device: %s (%s, %s)\n", info.deviceName != nullptr ? info.deviceName : "unknown",
      info.vendor != nullptr ? info.vendor : "unknown",
      info.apiVersion != nullptr ? info.apiVersion : "unknown");
    // What the display offers, which is the input to any future HDR output stage: the canvas presents
    // in sRGB today, and these two flags are how an application learns the surface could do more.
    const backend::Capabilities& capabilities = benchmarkDevice->getCapabilities();
    const coral::ui::FrameStatistics windowStatistics = window->frameStatistics();
    std::printf("  color    displayP3=%d hdr10=%d windowHdr=%d\n",
      capabilities.displayP3Swapchain ? 1 : 0, capabilities.hdr10Swapchain ? 1 : 0,
      windowStatistics.hdrOutput ? 1 : 0);
    if (windowStatistics.hdrOutput == false) {
      std::printf("  color    diagnostics: %s\n", window->diagnostics().c_str());
    }
  }
  std::printf("frame_benchmark %dx%d scale=%.2f rows=%d frames=%d relayout=%d dirty=%d native=%d raster=%d nodes=%zu build=%s\n",
    width, height, scale, rows, frames, relayout ? 1 : 0, (dirty || dirtyChild) ? 1 : 0, native ? 1 : 0, raster ? 1 : 0,
    nodes, kBuildType);
  if (!dirty && !relayout) {
    std::printf("  note     draw is 0 in this mode: a static tree hits the retained fast path, so only\n"
                "           present+total are meaningful. Pass --dirty to time the drawing pass.\n");
  }
  if (clips > 0) {
    std::printf("  scene    %d clip panels, clip_mode=%s, panel_fraction=%.2f\n", clips,
      clipModeName(clipMode), panelFraction);
  }
  if (cacheScene > 0) {
    std::printf("  scene    %dx%d recording-cache panels, %dx%d text tiles each, limit %d px\n",
      cacheScene, cacheScene, cacheTiles, cacheTiles, cacheLimitPx);
  }
  if (virtualProvider != nullptr) {
    std::printf("  scene    virtual list: %d rows, %zu realized, %zu pooled nodes\n", virtualRows,
      virtualProvider->poolSize(), nodes);
  }
  if (cacheHeavy > 0) {
    std::printf("  scene    %d heavy-paint panels, limit %d px\n", cacheHeavy, cacheLimitPx);
  }
  if (cacheScene > 0 || cacheHeavy > 0) {
    std::printf("  cache    %zu entries, %.1f KiB, budget %d MiB\n",
      coral::ui::Layer::recordingCacheEntryCount(),
      static_cast<double>(coral::ui::Layer::recordingCacheBytesInUse()) / 1024.0, cacheBudgetMiB);
  }
  const double measured = static_cast<double>(std::max<std::size_t>(1, total.size()));
  std::printf("  counters dirty avg=%5.1f max=%u  layout_dirty avg=%5.1f max=%u  "
              "repainted_boundaries avg=%5.1f max=%u  frame_list avg=%7.1f KiB  "
              "plan_items avg=%5.1f  composite avg=%5.1f  present_items avg=%5.1f  lent=%7.1f KiB  culled avg=%6.1f  damage avg=%5.1f%%\n",
    dirtySum / measured, dirtyMax, layoutDirtySum / measured, layoutDirtyMax,
    repaintedSum / measured, repaintedMax, frameListSum / measured / 1024.0,
    planItemSum / measured, compositeItemSum / measured, presentItemSum / measured,
    lentByteSum / measured / 1024.0, culledSum / measured,
    100.0 * damageSum / measured);
  std::printf("  gpu      command_buffers avg=%5.1f  (one per list unless they fit one frame)\n",
    commandBufferSum / measured);
  if (traceStages) {
    // The renderer's stage totals, summed over every list of the frame: this is what says where a
    // frame's CPU milliseconds are, which a per-list report cannot (a plan frame is several lists).
    std::printf("  stages   fields=%.3f precompile=%.3f arena=%.3f upload=%.3f record=%.3f "
                "execute=%.3f total=%.3f ms\n",
      stageFieldsSum / measured, stagePrecompileSum / measured, stageArenaSum / measured,
      stageVertexUploadSum / measured, stageRecordSum / measured, stageExecuteSum / measured,
      stageTotalSum / measured);
    std::printf("  stages   draw_calls=%.1f batches=%.1f\n", drawCallSum / measured,
      batchSum / measured);
    std::printf("  draws    rect=%.3f gradient=%.3f image=%.3f path=%.3f field=%.3f composite=%.3f ms\n",
      stageRectSum / measured, stageGradientSum / measured, stageImageSum / measured,
      stagePathSum / measured, stageFieldSum / measured, stageCompositeSum / measured);
  }
  // Text is the pipeline's most cache-sensitive stage: a run is a whole-line bitmap, so a frame that
  // cannot reuse its runs rebuilds every line it draws. Printing the rate per frame is what separates
  // "the scene draws more" from "the cache stopped working".
  std::printf("  text     runs=%.1f/frame  cache_hits=%.1f/frame  draws=%.1f/frame  built=%.1f MiB total  "
              "cache=%zu entries / %.1f MiB (budget %.1f MiB), evictions=%zu\n",
    static_cast<double>(textStats.runBuilds) / textFrames,
    static_cast<double>(textStats.runCacheHits) / textFrames,
    static_cast<double>(textStats.runDrawCalls) / textFrames,
    static_cast<double>(textStats.bitmapBytes) / (1024.0 * 1024.0),
    textStats.runCacheEntries,
    static_cast<double>(textStats.runCacheBytes) / (1024.0 * 1024.0),
    static_cast<double>(textStats.runCacheBudgetBytes) / (1024.0 * 1024.0),
    textStats.runCacheEvictions);
  report("update", update);
  report("layout", layout);
  report("draw", draw);
  report("paint", paint);
  report("present", present);
  report("total", total);
  // The GPU clock is the one number the CPU-side stages cannot give: it is read from the timestamp
  // queries the renderer records into each frame, one completed frame behind.
  const coral::ui::FrameStatistics gpuStatistics = window->frameStatistics();
  if (gpuStatistics.lastGpuMilliseconds > 0.0) {
    std::printf("  gpu      last completed frame=%.3f ms\n", gpuStatistics.lastGpuMilliseconds);
  }
  if (budgetVramMiB > 0.0) {
    // Live device memory, not the frame's upload bandwidth: what the scene keeps resident is what a
    // GPU budget is about, and the renderer's atlases and pools are what dominate it.
    const double vramMiB = static_cast<double>(gpuStatistics.residentBytes) / (1024.0 * 1024.0);
    const bool pass = vramMiB <= budgetVramMiB;
    std::printf("  vram     %.2f MiB resident %s budget %.2f MiB\n", vramMiB, pass ? "<=" : ">",
      budgetVramMiB);
    if (!pass) return 4;
  }
  if (budgetHandleGrowth > 0) {
    // A frame loop that leaks a handle per frame is invisible until the process hits the limit; the
    // delta across the measured run is the cheapest way to catch it. Windows counts handles, Linux
    // open descriptors, and both come from the same `processStatistics`.
    const coral::ui::ProcessStatistics end = coral::ui::processStatistics();
    const long long growth = static_cast<long long>(end.handles) -
      static_cast<long long>(warmupGrowStart.handles);
    const bool pass = growth <= budgetHandleGrowth;
    std::printf("  handles  grew %lld over %d frames %s budget %d\n", growth, frames, pass ? "<=" : ">",
      budgetHandleGrowth);
    if (!pass) return 5;
  }
  const double p95Total = percentile(total, 0.95);
  const double meanTotal = std::accumulate(total.begin(), total.end(), 0.0) /
                           static_cast<double>(total.size());
  std::printf("  budget   frame budget=%6.3f ms (60 Hz), p95 total=%.3f ms, headroom=%.2fx\n",
    1000.0 / 60.0, p95Total, (1000.0 / 60.0) / p95Total);
  if (budgetMs > 0.0) {
    double measured = p95Total;
    const char* metricName = "p95";
    if (budgetMetric == BudgetMetric::P50) {
      measured = percentile(total, 0.50);
      metricName = "p50";
    } else if (budgetMetric == BudgetMetric::Mean) {
      measured = meanTotal;
      metricName = "mean";
    }
    const bool pass = measured <= budgetMs;
    std::printf("  verdict  %s total %.3f ms %s budget %.3f ms\n", metricName, measured,
      pass ? "<=" : ">", budgetMs);
    if (!pass) return 2;
  }
  if (holdMs > 0) {
    // Acceptance aid: keep the window on screen so a person can look at it (and a screenshot can be
    // taken) after the measured frames, instead of the process exiting the moment the numbers print.
    std::printf("  hold     keeping the window for %d ms\n", holdMs);
    std::fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(holdMs));
  }
  if (budgetGrowMiB > 0.0) {
    // A steady-state run allocates almost nothing after warm-up, so growth is a leak signal. This is
    // process memory, not GPU residency: the renderer does not track device allocations yet.
    const coral::ui::ProcessStatistics end = coral::ui::processStatistics();
    const double growthMiB = (static_cast<double>(end.workingSetBytes) -
      static_cast<double>(warmupGrowStart.workingSetBytes)) / (1024.0 * 1024.0);
    const bool pass = growthMiB <= budgetGrowMiB;
    std::printf("  memory   working set grew %.2f MiB over %d frames %s budget %.2f MiB\n",
      growthMiB, frames, pass ? "<=" : ">", budgetGrowMiB);
    if (!pass) return 3;
  }
  return 0;
}
