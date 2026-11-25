#!/usr/bin/env bash
# Local CI entry point for the macOS capability work.
#
# Configures the arm64 preset, builds the framework tests + native windows, runs CTest, then builds
# the demo bundle. Use `--universal` to also build the lipo'd dual-architecture demo.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
CMAKE="${CMAKE:-cmake}"
JOBS="${JOBS:-8}"
UNIVERSAL=0
if [[ "${1:-}" == "--universal" ]]; then
  UNIVERSAL=1
fi

"$CMAKE" --preset macos-arm64
"$CMAKE" --build --preset macos-arm64 -j "$JOBS" \
  --target test_ui ui_native_smoke ui_native_ax_smoke ui_demo ui_mvvm_demo
ctest --test-dir out/macos-arm64 --output-on-failure \
  -R "^(test_ui|test_ui_shared_device|ui_native_smoke|ui_native_ax_smoke|example_ui_demo_headless|example_ui_demo_light_headless|example_ui_mvvm_headless)$"
"$ROOT/scripts/check-install.sh" out/macos-arm64

if [[ "$UNIVERSAL" -eq 1 ]]; then
  "$ROOT/scripts/make-universal-demo.sh"
fi
