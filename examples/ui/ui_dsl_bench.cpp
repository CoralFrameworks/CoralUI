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
 * @file ui_dsl_bench.cpp
 *
 * A small, repeatable baseline for the DSL's Element build + Reconciler path. It is not a general UI
 * benchmark: it deliberately measures the pieces a DSL page exercises every state update (tree build,
 * reconcile, layout and paint) plus an Element-content overlay open/close.
 *
 *   ui_dsl_bench [--iterations N] [--rows N]
 */

#include <ui/DSL.h>
#include <ui/ElementHost.h>
#include <ui/Theme.h>
#include <ui/Window.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using namespace coral::ui;

ElementPtr buildPage(const int rows, const int tick) {
  std::vector<int> items(static_cast<size_t>(rows));
  for (int index = 0; index < rows; ++index) items[static_cast<size_t>(index)] = index;
  return dsl::Page("DSL benchmark",
    dsl::Caption("tick " + std::to_string(tick)),
    dsl::ForEach(items, [tick](const int row, const size_t index) {
      const bool accent = ((row + tick) % 5) == 0;
      return dsl::Row(dsl::StackOptions{ .gap = 6.0f, .padding = Insets{}.all(2.0f),
        .key = "row." + std::to_string(index) },
        dsl::Swatch(accent ? Color::MakeRGBA8(86, 156, 255, 255)
                           : Color::MakeRGBA8(62, 67, 78, 255), 8.0f, 8.0f, 2.0f,
          "row." + std::to_string(index) + ".dot"),
        dsl::Body("row " + std::to_string(row) + " / tick " + std::to_string(tick),
          "row." + std::to_string(index) + ".text"));
    }, dsl::StackOptions{ .gap = 2.0f, .key = "rows" }));
}

} // namespace

int main(int argc, char** argv) {
  int iterations = 100;
  int rows = 160;
  bool json = false;
  double maxAverageMs = 0.0;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--iterations" && index + 1 < argc) {
      iterations = std::max(1, std::atoi(argv[++index]));
    } else if (argument == "--rows" && index + 1 < argc) {
      rows = std::max(1, std::atoi(argv[++index]));
    } else if (argument == "--json") {
      json = true;
    } else if (argument == "--max-avg-ms" && index + 1 < argc) {
      maxAverageMs = std::max(0.0, std::atof(argv[++index]));
    } else if (argument == "--help") {
      std::printf("usage: ui_dsl_bench [--iterations N] [--rows N] [--json] [--max-avg-ms N]\n");
      return 0;
    }
  }

  Window::Options windowOptions;
  windowOptions.width = 1180;
  windowOptions.height = 780;
  windowOptions.headless = true;
  windowOptions.frameLimit = 0;
  windowOptions.title = "CoralUI DSL benchmark";
  auto window = Window::create(windowOptions);
  if (window == nullptr) {
    std::fprintf(stderr, "ui_dsl_bench: could not create a headless window\n");
    return 1;
  }

  int tick = 0;
  ElementHost host(*window, [&] { return buildPage(rows, ++tick); });
  host.mount();
  window->setRoot(host.root());
  window->requestFrame();
  window->renderFrame();

  const auto buildStart = std::chrono::steady_clock::now();
  for (int index = 0; index < iterations; ++index) {
    host.refresh();
    window->pump();
    window->requestFrame();
    window->renderFrame();
  }
  const auto buildEnd = std::chrono::steady_clock::now();
  const double buildMs = std::chrono::duration<double, std::milli>(buildEnd - buildStart).count();

  dsl::Overlay overlays(*window, Theme::dark());
  OverlayOptions anchored;
  anchored.anchor = Rect::MakeXYWH(20.0f, 20.0f, 160.0f, 32.0f);
  const auto overlayStart = std::chrono::steady_clock::now();
  for (int index = 0; index < iterations; ++index) {
    const uint64_t id = overlays.PopupElement(
      dsl::Panel(dsl::Text("popup " + std::to_string(index))), anchored);
    if (id != 0) overlays.close(id);
  }
  const auto overlayEnd = std::chrono::steady_clock::now();
  const double overlayMs = std::chrono::duration<double, std::milli>(overlayEnd - overlayStart).count();

  const double rebuildAverage = buildMs / static_cast<double>(iterations);
  const double overlayAverage = overlayMs / static_cast<double>(iterations);
  if (json) {
    std::printf("{\"rows\":%d,\"iterations\":%d,\"rebuild_total_ms\":%.3f,"
      "\"rebuild_avg_ms\":%.3f,\"overlay_total_ms\":%.3f,\"overlay_avg_ms\":%.3f}\n",
      rows, iterations, buildMs, rebuildAverage, overlayMs, overlayAverage);
  } else {
    std::printf("dsl-bench rows=%d iterations=%d\n", rows, iterations);
    std::printf("  rebuild+layout+paint total=%.2fms avg=%.3fms\n", buildMs, rebuildAverage);
    std::printf("  popup Element open+close total=%.2fms avg=%.3fms\n", overlayMs, overlayAverage);
  }
  if (buildMs <= 0.0 || overlayMs <= 0.0) return 1;
  if (maxAverageMs > 0.0 && (rebuildAverage > maxAverageMs || overlayAverage > maxAverageMs)) {
    std::fprintf(stderr, "ui_dsl_bench: average exceeded the %.3fms budget\n", maxAverageMs);
    return 2;
  }
  return 0;
}
