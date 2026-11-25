#!/usr/bin/env bash
# Pre-notarization checks for a Developer ID built .app.
#
# Usage: scripts/check-macos-release.sh [App.app] [keychain-profile]
#
# It verifies the Developer ID authority, hardened runtime, universal slices and Gatekeeper assessment.
# The optional keychain profile is used only to check notarytool can authenticate.
set -euo pipefail

APP="${1:-out/examples/ui/ui_demo.app}"
PROFILE="${2:-}"

if [[ ! -d "$APP" ]]; then
  echo "not a .app bundle: $APP" >&2
  exit 2
fi

codesign --verify --deep --strict --verbose=2 "$APP"

SIGNATURE="$(codesign -dvv "$APP" 2>&1)"
if ! grep -q 'Authority=Developer ID Application' <<<"$SIGNATURE"; then
  echo "not signed with a Developer ID Application certificate" >&2
  echo "$SIGNATURE" | sed -n '1,8p' >&2
  exit 1
fi
if ! grep -q 'flags=.*runtime' <<<"$SIGNATURE"; then
  echo "hardened runtime flag is missing" >&2
  exit 1
fi

ARCHS="$(lipo -archs "$APP/Contents/MacOS/$(basename "$APP" .app)")"
if [[ "$ARCHS" != *arm64* || "$ARCHS" != *x86_64* ]]; then
  echo "expected universal app, got: $ARCHS" >&2
  exit 1
fi

if ! spctl --assess --type execute --verbose=2 "$APP" 2>&1; then
  echo "Gatekeeper assessment failed; submit/notarize the app first" >&2
fi

if [[ -n "$PROFILE" ]]; then
  xcrun notarytool history --keychain-profile "$PROFILE" >/dev/null
  echo "notarytool profile '$PROFILE' is available"
fi

echo "release checks passed for $APP ($ARCHS)"
