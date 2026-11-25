#!/usr/bin/env bash
# Installs an existing build tree into a temporary prefix and checks the public SDK layout.
#
# Usage: scripts/check-install.sh [build-dir]
set -euo pipefail

BUILD_DIR="${1:-out/examples}"
PREFIX="$(mktemp -d "${TMPDIR:-/tmp}/coral-ui-install.XXXXXX")"
trap 'rm -rf "$PREFIX"' EXIT

cmake --build "$BUILD_DIR" -j "${JOBS:-4}" >/dev/null
cmake --install "$BUILD_DIR" --prefix "$PREFIX" >/dev/null

for header in ui/Rating.h ui/Toolbar.h ui/StatusBar.h ui/ProgressRing.h; do
  if [[ ! -f "$PREFIX/include/$header" ]]; then
    echo "missing installed header: $header" >&2
    exit 1
  fi
done

if [[ ! -f "$PREFIX/lib/cmake/CoralUI/CoralUIConfig.cmake" ]]; then
  echo "missing installed CoralUI CMake package" >&2
  exit 1
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
  if [[ ! -f "$PREFIX/include/ui/macos/NativeControlSize.h" ]]; then
    echo "missing macOS native headers" >&2
    exit 1
  fi
fi

echo "install layout ok"
