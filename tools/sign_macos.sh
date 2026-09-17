#!/usr/bin/env bash
#
# Signs, notarises and staples the macOS build. Runs on a macOS runner in CI, where the
# Developer ID lives in secrets; it can also be run locally by anyone who holds the
# certificate. Nothing here echoes a secret.
#
#   tools/sign_macos.sh <artefacts dir>          e.g. build/AETHER_artefacts/Release
#
# Environment:
#   IDENTITY            Developer ID Application: ... (TEAMID)     [required]
#   APPLE_ID            Apple ID for notarisation                  [required]
#   APPLE_PASSWORD      app-specific password                      [required]
#   TEAM_ID             10-character team id                       [required]
#
# The .pkg is built separately by installer/macos/build_pkg.sh.

set -euo pipefail

ARTEFACTS="${1:?usage: sign_macos.sh <artefacts dir>}"
: "${IDENTITY:?IDENTITY is required}"
: "${APPLE_ID:?APPLE_ID is required}"
: "${APPLE_PASSWORD:?APPLE_PASSWORD is required}"
: "${TEAM_ID:?TEAM_ID is required}"

VERSION="${VERSION:-1.0.0}"
DIST="$(cd "$(dirname "$0")/.." && pwd)/dist"
mkdir -p "$DIST"

sign_bundle() {
    local bundle="$1"
    [ -d "$bundle" ] || return 0
    echo "Signing $(basename "$bundle")"
    # Hardened runtime is required for notarisation. Timestamped, and deep so the
    # framework-style bundle's inner binary is covered.
    codesign --force --deep --options runtime --timestamp \
             --sign "$IDENTITY" "$bundle"
    codesign --verify --strict --verbose=2 "$bundle"
}

VST3="$ARTEFACTS/VST3/AETHER.vst3"
AU="$ARTEFACTS/AU/AETHER.component"
sign_bundle "$VST3"
sign_bundle "$AU"

# Notarisation takes a zip or a pkg, never a bare bundle.
ZIP="$DIST/AETHER-macOS-$VERSION.zip"
rm -f "$ZIP"
STAGE="$(mktemp -d)"
[ -d "$VST3" ] && cp -R "$VST3" "$STAGE/"
[ -d "$AU" ]   && cp -R "$AU" "$STAGE/"
( cd "$STAGE" && /usr/bin/ditto -c -k --keepParent . "$ZIP" )

echo "Submitting for notarisation (this waits for Apple)"
xcrun notarytool submit "$ZIP" \
    --apple-id "$APPLE_ID" --password "$APPLE_PASSWORD" --team-id "$TEAM_ID" \
    --wait --timeout 30m

# The ticket is stapled to the bundles, not the zip, so re-zip afterwards.
[ -d "$VST3" ] && xcrun stapler staple "$VST3"
[ -d "$AU" ]   && xcrun stapler staple "$AU"

rm -rf "$STAGE"; STAGE="$(mktemp -d)"
[ -d "$VST3" ] && cp -R "$VST3" "$STAGE/"
[ -d "$AU" ]   && cp -R "$AU" "$STAGE/"
rm -f "$ZIP"
( cd "$STAGE" && /usr/bin/ditto -c -k --keepParent . "$ZIP" )
rm -rf "$STAGE"
echo "Stapled bundles: $ZIP"

# The .pkg is NOT built here. Packaging belongs to installer/macos/build_pkg.sh, which
# runs on every build whether or not signing secrets exist; burying it behind this script's
# INSTALLER_IDENTITY check is why no run ever produced a macOS installer.
