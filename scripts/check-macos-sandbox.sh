#!/usr/bin/env bash
# Sign a demo bundle with the App Sandbox entitlement template and verify the signature.
#
# Usage: scripts/check-macos-sandbox.sh [App.app] [identity]
#   App.app   defaults to out/examples/ui/ui_demo.app
#   identity  defaults to "-" (ad-hoc); use a Developer ID to exercise the full release path.
#
# This proves the entitlements/signature plumbing. ui_demo now bundles its reference font, so the
# headless launch works under the sandbox. `ui_demo --bookmark-smoke` additionally exercises a
# container-local security-scoped bookmark; user-selected file access still needs a real NSOpenPanel.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="${1:-$ROOT/out/examples/ui/ui_demo.app}"
IDENTITY="${2:--}"
ENTITLEMENTS="${CORAL_MACOS_ENTITLEMENTS:-$ROOT/examples/ui/CoralUIDemo.entitlements}"

if [[ ! -d "$APP" ]]; then
  echo "not a .app bundle: $APP" >&2
  exit 2
fi
if [[ ! -f "$ENTITLEMENTS" ]]; then
  echo "missing entitlements: $ENTITLEMENTS" >&2
  exit 2
fi

SIGN_ARGS=(--force --sign "$IDENTITY" --options runtime --entitlements "$ENTITLEMENTS")
if [[ "$IDENTITY" == "-" ]]; then
  SIGN_ARGS+=(--timestamp=none)
else
  SIGN_ARGS+=(--timestamp)
fi
codesign "${SIGN_ARGS[@]}" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

ENT_PLIST="$(mktemp)"
trap 'rm -f "$ENT_PLIST"' EXIT
codesign -d --entitlements :- "$APP" > "$ENT_PLIST" 2>/dev/null
if ! grep -q 'com.apple.security.app-sandbox' "$ENT_PLIST"; then
  echo "sandbox entitlement was not embedded" >&2
  exit 1
fi
if ! grep -q 'com.apple.security.files.user-selected.read-write' "$ENT_PLIST"; then
  echo "user-selected file entitlement was not embedded" >&2
  exit 1
fi

echo "verified:"
codesign -dvv "$APP" 2>&1 | sed -n '1,8p'
codesign -d --entitlements :- "$APP" 2>/dev/null
