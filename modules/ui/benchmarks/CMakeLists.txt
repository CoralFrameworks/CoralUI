add_executable(text_benchmark text_benchmark.cpp)
target_link_libraries(text_benchmark PRIVATE ui minilog)
target_compile_definitions(text_benchmark PRIVATE CORAL_ASSETS_DIR="${CORAL_TEST_DATA_DIR}")
if (MSVC)
  target_compile_options(text_benchmark PRIVATE /W4 /utf-8)
else ()
  target_compile_options(text_benchmark PRIVATE -Wall -Wextra)
endif ()
set_target_properties(text_benchmark PROPERTIES FOLDER Benchmarks)

add_executable(frame_benchmark frame_benchmark.cpp)
target_link_libraries(frame_benchmark PRIVATE ui minilog)
target_compile_definitions(frame_benchmark PRIVATE CORAL_ASSETS_DIR="${CORAL_TEST_DATA_DIR}")
if (MSVC)
  target_compile_options(frame_benchmark PRIVATE /W4 /utf-8)
else ()
  target_compile_options(frame_benchmark PRIVATE -Wall -Wextra)
endif ()
set_target_properties(frame_benchmark PROPERTIES FOLDER Benchmarks)

add_executable(idle_benchmark idle_benchmark.cpp)
target_link_libraries(idle_benchmark PRIVATE ui minilog)
if (MSVC)
  target_compile_options(idle_benchmark PRIVATE /W4 /utf-8)
else ()
  target_compile_options(idle_benchmark PRIVATE -Wall -Wextra)
endif ()
set_target_properties(idle_benchmark PROPERTIES FOLDER Benchmarks)

if (WIN32)
  # Acceptance aid: renders the same shaped run through DirectWrite's ClearType bitmap target and
  # through the UI pipeline, then writes side-by-side and difference images.
  add_executable(cleartype_reference cleartype_reference.cpp)
  target_link_libraries(cleartype_reference PRIVATE ui minilog dwrite gdi32)
  target_include_directories(cleartype_reference PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
  target_compile_definitions(cleartype_reference PRIVATE CORAL_ASSETS_DIR="${CORAL_TEST_DATA_DIR}")
  if (MSVC)
    target_compile_options(cleartype_reference PRIVATE /W4 /utf-8)
  endif ()
  set_target_properties(cleartype_reference PROPERTIES FOLDER Benchmarks)
endif ()

# The P0-4b gates. They run the real window loop, and `--allow-no-gpu` turns "this machine has no
# device" into a skip rather than a failure, which is what lets the same test live on a hosted runner
# and on a GPU workstation. Budgets are regression guards, not targets: they sit several times above
# the measured cost. Build type matters more than any threshold here -- the same scene costs 15-20x
# more in a debug build -- so the scroll/repaint gates below pass `--require-release`; the measured
# values live in docs/GUI基准数据.md §8, and the reason for both rules is in
# docs/GUI保留模式现状复核与改造路线.md §2 and §6 (R8).
add_test(NAME ui_frame_budget COMMAND $<TARGET_FILE:frame_benchmark>
  --rows 240 --frames 120 --warmup 300 --budget-ms 6 --budget-metric p50 --allow-no-gpu)
set_tests_properties(ui_frame_budget PROPERTIES LABELS perf)

add_test(NAME ui_clip_rounded_budget COMMAND $<TARGET_FILE:frame_benchmark>
  --clips 8 --clip-mode rounded --frames 60 --warmup 300 --budget-ms 6 --budget-metric p50 --dirty --allow-no-gpu)
set_tests_properties(ui_clip_rounded_budget PROPERTIES LABELS perf)

add_test(NAME ui_clip_path_budget COMMAND $<TARGET_FILE:frame_benchmark>
  --clips 4 --clip-mode path --frames 30 --warmup 200 --budget-ms 8 --budget-metric p50 --dirty --allow-no-gpu)
set_tests_properties(ui_clip_path_budget PROPERTIES LABELS perf)

# 4K / 8 path clips is a known limitation rather than a must-pass scene (2026-11-13 decision). It is
# registered as a reporting benchmark, not a `perf` gate: `ctest -L perf` must not fail on it, but a
# manual or dashboard run still records the number (11.3 ms when this was written).
add_test(NAME ui_clip_path_4k_report COMMAND $<TARGET_FILE:frame_benchmark>
  --width 3840 --height 2160 --clips 8 --clip-mode path --frames 20 --warmup 100 --dirty --allow-no-gpu)
set_tests_properties(ui_clip_path_4k_report PROPERTIES LABELS "benchmark;report")

# A steady-state run must not grow the process: after warm-up almost nothing allocates, so growth is a
# leak (a texture pool that keeps growing, a command buffer dropped per frame). This is process memory,
# not GPU residency; `ui_vram_budget` below is the residency gate.
add_test(NAME ui_memory_growth COMMAND $<TARGET_FILE:frame_benchmark>
  --rows 240 --frames 600 --warmup 300 --budget-grow-mib 64 --allow-no-gpu)
set_tests_properties(ui_memory_growth PROPERTIES LABELS perf)

# Live device memory (textures + buffers) for the editor scene, counted at the backend resource tables
# (Vulkan allocation sizes / GL format+mip size). The ceiling is generous so a driver's internal
# padding or a different GPU does not turn a regression guard into a flaky test; the measured value is
# about 22 MiB on Vulkan and 21 MiB on GL.
add_test(NAME ui_vram_budget COMMAND $<TARGET_FILE:frame_benchmark>
  --rows 240 --frames 120 --warmup 300 --budget-vram-mib 128 --allow-no-gpu)
set_tests_properties(ui_vram_budget PROPERTIES LABELS perf)

# R-5: the gates used p50 because an idle GPU ramps its clocks mid-run, and a short p95 burst measures
# the governor. A long warm-up removes that excuse: by the time this runs the clocks are high, so p95
# is the tail a user feels and the threshold can be a real regression guard rather than a formality.
add_test(NAME ui_frame_p95_budget COMMAND $<TARGET_FILE:frame_benchmark>
  --rows 240 --frames 300 --warmup 300 --budget-ms 10 --budget-metric p95 --allow-no-gpu)
set_tests_properties(ui_frame_p95_budget PROPERTIES LABELS perf)

# The frame loop must not leak an OS handle or descriptor per frame; see `--budget-handle-growth`.
add_test(NAME ui_handle_growth COMMAND $<TARGET_FILE:frame_benchmark>
  --rows 240 --frames 600 --warmup 300 --budget-handle-growth 16 --allow-no-gpu)
set_tests_properties(ui_handle_growth PROPERTIES LABELS perf)

# R8: scrolling and full repaint are the two paths the retained-mode route is measured on, and neither
# was gated. Every other gate above runs the static tree, which measures only `present` because the
# retained short-circuit skips drawing. `--scroll` advances the viewport every frame, so it times the
# invalidation + recording path; `--dirty` forces the whole tree to re-record. Both are release-only:
# the same scenes cost 15-20x more in a debug build, and a threshold tuned for one is noise for the
# other.
add_test(NAME ui_scroll_budget COMMAND $<TARGET_FILE:frame_benchmark>
  --rows 240 --frames 120 --warmup 120 --scroll --budget-ms 8 --budget-metric p95 --allow-no-gpu --require-release)
set_tests_properties(ui_scroll_budget PROPERTIES LABELS perf)

add_test(NAME ui_repaint_budget COMMAND $<TARGET_FILE:frame_benchmark>
  --rows 240 --frames 120 --warmup 120 --dirty --budget-ms 6 --budget-metric p95 --allow-no-gpu --require-release)
set_tests_properties(ui_repaint_budget PROPERTIES LABELS perf)

# The large-list scene is what exposed the text-run cache cliff: the same tree was linear up to about
# two thousand rows and then 15x slower (docs/GUI保留模式现状复核与改造路线.md §4.4). R9 replaced the
# run cache's clear-all policy with a byte budget, which is what the budget below guards; the scene
# still needs culling above roughly three thousand rows (R10), which is why the budget is generous.
add_test(NAME ui_scroll_large_budget COMMAND $<TARGET_FILE:frame_benchmark>
  --rows 2400 --frames 10 --warmup 5 --scroll --budget-ms 150 --budget-metric p95 --allow-no-gpu --require-release)
set_tests_properties(ui_scroll_large_budget PROPERTIES LABELS perf)
