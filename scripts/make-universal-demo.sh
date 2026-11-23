#!/usr/bin/env bash
# Builds the arm64 and x86_64 slices of the demo and lipos the executable into one .app.
#
# A single configure cannot build universal here: CoralGraphics' vendored libjpeg-turbo rejects
# multiple CMAKE_OSX_ARCHITECTURES. Building two slices keeps the third-party trees unchanged; the
# frameworks are static, so lipoing the demo executable is enough for this bundle.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CMAKE="${CMAKE:-cmake}"
"$CMAKE" --preset macos-arm64
"$CMAKE" --build --preset macos-arm64 --target ui_demo
# The x86 slice cannot run its own shader_compiler on an arm host; reuse the host slice's tool.
"$CMAKE" --preset macos-x86_64
"$CMAKE" --build --preset macos-x86_64 --target ui_demo

ARM_APP="out/macos-arm64/examples/ui/ui_demo.app"
X86_APP="out/macos-x86_64/examples/ui/ui_demo.app"
OUT_APP="out/universal/ui_demo.app"
rm -rf "out/universal"
mkdir -p "out/universal"
cp -R "$ARM_APP" "$OUT_APP"
lipo -create "$ARM_APP/Contents/MacOS/ui_demo" "$X86_APP/Contents/MacOS/ui_demo" \
  -output "$OUT_APP/Contents/MacOS/ui_demo"
lipo -info "$OUT_APP/Contents/MacOS/ui_demo"
