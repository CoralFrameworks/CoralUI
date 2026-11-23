#!/usr/bin/env bash
# Submits a signed .app to Apple's notary service and staples the ticket.
#
# Usage: scripts/notarize-macos-app.sh path/to/App.app <keychain-profile>
# The app must already carry a Developer ID signature with the hardened runtime.
set -euo pipefail

APP="${1:?usage: $0 path/to/App.app <notarytool-keychain-profile>}"
PROFILE="${2:?usage: $0 path/to/App.app <notarytool-keychain-profile>}"
if [[ ! -d "$APP" ]]; then
  echo "not a .app bundle: $APP" >&2
  exit 2
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
ZIP="$WORK/$(basename "$APP").zip"
ditto -c -k --keepParent "$APP" "$ZIP"
xcrun notarytool submit "$ZIP" --keychain-profile "$PROFILE" --wait
xcrun stapler staple "$APP"
xcrun stapler validate "$APP"
