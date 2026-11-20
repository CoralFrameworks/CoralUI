/*
 * Copyright (C) 2026 The Coral Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Idle CPU and input-to-present latency for a native OnDemand window.
 *
 * Idle phase: warm one frame, then run the real loop with no input for --idle-ms and measure process
 * CPU time. The loop must block on the display/self-pipe, not poll.
 *
 * Latency phase: a producer thread posts a frame request and measures the wall time until the frame
 * callback announces that present returned. This is the event-to-present path the self-pipe exists
 * to shorten; it is a host-side measurement, not a hardware input timestamp.
 */

#include <ui/ColorLayer.h>
#include <ui/Application.h>
#include <ui/Window.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <ctime>
#endif

namespace {

using Clock = std::chrono::steady_clock;

double processCpuSeconds() {
#if defined(_WIN32)
  FILETIME creation{}, exit{}, kernel{}, user{};
  if (GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user) == 0) return 0.0;
  const auto toSeconds = [](const FILETIME& time) {
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return static_cast<double>(value.QuadPart) * 1.0e-7;
  };
  return toSeconds(kernel) + toSeconds(user);
#else
  timespec time{};
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time) != 0) return 0.0;
  return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_nsec) * 1.0e-9;
#endif
}

double percentile(std::vector<double> values, const double fraction) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const size_t index = std::min(values.size() - 1,
    static_cast<size_t>(fraction * static_cast<double>(values.size() - 1)));
  return values[index];
}

struct Arguments {
  int idleMs = 2000;
  int samples = 30;
  int width = 256;
  int height = 256;
  bool application = false;
  int windows = 2;
};

Arguments parse(int argc, char** argv) {
  Arguments args;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--idle-ms" && index + 1 < argc) args.idleMs = std::atoi(argv[++index]);
    else if (argument == "--samples" && index + 1 < argc) args.samples = std::atoi(argv[++index]);
    else if (argument == "--width" && index + 1 < argc) args.width = std::atoi(argv[++index]);
    else if (argument == "--height" && index + 1 < argc) args.height = std::atoi(argv[++index]);
    else if (argument == "--application") args.application = true;
    else if (argument == "--windows" && index + 1 < argc) args.windows = std::max(1, std::atoi(argv[++index]));
    else if (argument == "--help") {
      std::printf("usage: idle_benchmark [--idle-ms N] [--samples N] [--width W] [--height H]\n"
                  "                     [--application] [--windows N]\n");
      std::exit(0);
    }
  }
  args.samples = std::max(1, args.samples);
  return args;
}

std::shared_ptr<coral::ui::Window> makeWindow(const Arguments& args) {
  coral::ui::Window::Options options;
  options.headless = false;
  options.width = args.width;
  options.height = args.height;
  options.framePolicy = coral::ui::FramePolicy::OnDemand;
  auto window = coral::ui::Window::create(options);
  if (window != nullptr) {
    window->setRoot(std::make_shared<coral::ui::ColorLayer>(
      graphics::Color::MakeRGBA8(30, 34, 42, 255)));
  }
  return std::shared_ptr<coral::ui::Window>(std::move(window));
}

/**
 * Idle CPU for the multi-window `Application` loop.
 *
 * The single-window path blocks in `WindowPlatform::waitForEvents`; the application path has no
 * shared platform event source yet, so it waits on its task queue with a bounded timeout. This mode
 * is what shows that the loop is not spinning at 100% CPU while it waits.
 */
int runApplicationIdle(const Arguments& args) {
  auto application = coral::ui::Application::create();
  if (application == nullptr) return 1;

  std::vector<std::shared_ptr<coral::ui::Window>> windows;
  int warmFrames = 0;
  for (int index = 0; index < args.windows; ++index) {
    coral::ui::Window::Options options;
    options.headless = false;
    options.width = args.width;
    options.height = args.height;
    options.framePolicy = coral::ui::FramePolicy::OnDemand;
    auto window = application->createWindow(options);
    if (window == nullptr) {
      std::printf("application idle: no native window/GPU device; skipping\n");
      return 0;
    }
    window->setRoot(std::make_shared<coral::ui::ColorLayer>(graphics::Color::MakeRGBA8(30, 34, 42, 255)));
    if (!window->renderFrame()) {
      std::printf("application idle: warmup frame failed: %s\n", window->diagnostics().c_str());
      return 1;
    }
    warmFrames += window->frameCount();
    windows.push_back(std::move(window));
  }

  std::thread stopper([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(args.idleMs));
    application->quit();
  });
  const double cpuBefore = processCpuSeconds();
  const auto wallBefore = Clock::now();
  application->run();
  stopper.join();
  const auto wallAfter = Clock::now();
  const double cpuAfter = processCpuSeconds();
  const double wallSeconds = std::chrono::duration<double>(wallAfter - wallBefore).count();
  const double idleCpu = wallSeconds > 0.0 ? (cpuAfter - cpuBefore) / wallSeconds * 100.0 : 0.0;

  int frames = 0;
  for (const auto& window: windows) frames += window->frameCount();
  std::printf("application idle: %.1f ms wall, %.1f%% process CPU, windows=%zu frames=%d\n",
    wallSeconds * 1000.0, idleCpu, windows.size(), frames - warmFrames);
  return 0;
}

} // namespace

int main(int argc, char** argv) {
  const Arguments args = parse(argc, argv);
  if (args.application) return runApplicationIdle(args);
  auto window = makeWindow(args);
  if (window == nullptr) {
    std::printf("idle_benchmark: no native window/GPU device; skipping\n");
    return 0;
  }
  if (!window->renderFrame()) {
    std::printf("idle_benchmark: warmup frame failed: %s\n", window->diagnostics().c_str());
    return 1;
  }
  const int warmFrames = window->frameCount();

  std::thread stopper([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(args.idleMs));
    window->quit();
  });
  const double cpuBefore = processCpuSeconds();
  const auto wallBefore = Clock::now();
  window->run();
  stopper.join();
  const auto wallAfter = Clock::now();
  const double cpuAfter = processCpuSeconds();
  const double wallSeconds = std::chrono::duration<double>(wallAfter - wallBefore).count();
  const double idleCpu = wallSeconds > 0.0
    ? (cpuAfter - cpuBefore) / wallSeconds * 100.0 : 0.0;
  const int idleFrames = window->frameCount() - warmFrames;

  std::printf("idle: %.1f ms wall, %.1f%% process CPU, frames=%d\n",
    wallSeconds * 1000.0, idleCpu, idleFrames);

  // Latency: chain one posted frame request per sample. The timestamp is taken when the posted task
  // runs (UI thread), and the frame callback records present completion. This is task-to-present on
  // the UI thread; the idle phase above is what shows the loop is not spinning.
  auto latencyWindow = makeWindow(args);
  if (latencyWindow == nullptr) return 0;
  std::atomic<int64_t> requestedNanos{ 0 };
  int completed = 0;
  std::vector<double> latencies;
  latencies.reserve(static_cast<size_t>(args.samples));
  auto schedule = [&] {
    requestedNanos.store(std::chrono::duration_cast<std::chrono::nanoseconds>(
      Clock::now().time_since_epoch()).count(), std::memory_order_release);
    latencyWindow->post([&] { latencyWindow->requestFrame(); });
  };
  latencyWindow->setFrameCallback([&] {
    const int64_t requested = requestedNanos.exchange(0, std::memory_order_acq_rel);
    if (requested != 0) {
      const int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now().time_since_epoch()).count();
      latencies.push_back(static_cast<double>(now - requested) / 1.0e6);
      ++completed;
    }
    if (completed >= args.samples) {
      latencyWindow->quit();
    } else {
      schedule();
    }
  });
  schedule();
  latencyWindow->run();

  if (latencies.empty()) {
    std::printf("latency: no completed samples\n");
    return 1;
  }
  std::printf("latency n=%zu p50=%.3f ms p95=%.3f ms\n", latencies.size(),
    percentile(latencies, 0.50), percentile(latencies, 0.95));
  return 0;
}

