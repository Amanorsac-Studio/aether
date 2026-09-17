#!/usr/bin/env bash
#
# Builds AETHER-<version>-macOS.pkg: the single-file macOS installer required by the
# Installer & Packaging Standard (P1, P28).
#
# This runs on EVERY macOS build, signed or not. The .pkg used to be built inside
# tools/sign_macos.sh, which only ran when the signing secrets existed and only then if
# INSTALLER_IDENTITY was also set - so in practice no run ever produced an installer and
# the macOS artefact was a pair of bare bundles. Packaging and signing are separate
# concerns and are now separate scripts.
#
#   installer/macos/build_pkg.sh <artefacts dir> [version]
#
# Optional environment:
#   INSTALLER_IDENTITY   "Developer ID Installer: ... (TEAMID)" - signs the .pkg when set.
#                        Unset produces an unsigned .pkg, which is fine for local testing
#                        and refused for a release by the workflow's tag guard.

set -euo pipefail

ARTEFACTS="${1:?usage: build_pkg.sh <artefacts dir> [version]}"
VERSION="${2:-${VERSION:-1.0.0}}"

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
HERE="$REPO/installer/macos"
DIST="$REPO/dist"
mkdir -p "$DIST"

VST3="$ARTEFACTS/VST3/AETHER.vst3"
AU="$ARTEFACTS/AU/AETHER.component"
[ -d "$VST3" ] || { echo "error: no VST3 bundle at $VST3" >&2; exit 1; }

# P8: install the .vst3 as a bundle. P9/P10: only the standard locations, and nothing
# user-editable under /Library - presets live in ~/Documents (File & Data Conventions).
VST3_DEST="/Library/Audio/Plug-Ins/VST3"
AU_DEST="/Library/Audio/Plug-Ins/Components"
SUPPORT_DEST="/Library/Application Support/Amanorsac Studio/AETHER"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
PKGS="$WORK/pkgs"; mkdir -p "$PKGS"

# ---- one component package per format, so each gets its own checkbox (P12) ----
build_component() {
    local name="$1" src="$2" dest="$3" ident="$4"
    local root="$WORK/root-$name"
    mkdir -p "$root$dest"
    cp -R "$src" "$root$dest/"
    pkgbuild --root "$root" --identifier "$ident" --version "$VERSION" \
             --install-location / "$PKGS/$name.pkg" >/dev/null
    echo "  component: $name -> $dest"
}

build_component vst3 "$VST3" "$VST3_DEST" studio.amanorsac.aether.vst3
HAVE_AU=0
if [ -d "$AU" ]; then
    build_component au "$AU" "$AU_DEST" studio.amanorsac.aether.au
    HAVE_AU=1
fi

# P24: a .pkg leaves nothing behind to run, so ship the uninstaller as a payload item
# and document it in the Read Me.
UNROOT="$WORK/root-uninstall"
mkdir -p "$UNROOT$SUPPORT_DEST"
cp "$HERE/Uninstall AETHER.command" "$UNROOT$SUPPORT_DEST/"
chmod +x "$UNROOT$SUPPORT_DEST/Uninstall AETHER.command"
cp "$REPO/README.md" "$UNROOT$SUPPORT_DEST/"
pkgbuild --root "$UNROOT" --identifier studio.amanorsac.aether.support \
         --version "$VERSION" --install-location / "$PKGS/support.pkg" >/dev/null

# ---- distribution: the pages the buyer sees (P11 paths, P12 checkboxes) ----
DIST_XML="$WORK/distribution.xml"
{
cat <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>AETHER $VERSION</title>
    <organization>studio.amanorsac</organization>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <welcome    file="welcome.html"    mime-type="text/html"/>
    <license    file="license.html"    mime-type="text/html"/>
    <readme     file="readme.html"     mime-type="text/html"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <choices-outline>
        <line choice="choice_vst3"/>
XML
[ "$HAVE_AU" = 1 ] && echo '        <line choice="choice_au"/>'
cat <<XML
        <line choice="choice_support"/>
    </choices-outline>

    <choice id="choice_vst3" title="VST3 plug-in" visible="true"
            description="Installs AETHER.vst3 to $VST3_DEST">
        <pkg-ref id="studio.amanorsac.aether.vst3"/>
    </choice>
XML
[ "$HAVE_AU" = 1 ] && cat <<XML
    <choice id="choice_au" title="Audio Unit" visible="true"
            description="Installs AETHER.component to $AU_DEST">
        <pkg-ref id="studio.amanorsac.aether.au"/>
    </choice>
XML
cat <<XML
    <choice id="choice_support" title="Uninstaller and Read Me" visible="true"
            description="Installs Uninstall AETHER.command and README.md to $SUPPORT_DEST">
        <pkg-ref id="studio.amanorsac.aether.support"/>
    </choice>

    <pkg-ref id="studio.amanorsac.aether.vst3"    version="$VERSION" onConclusion="none">vst3.pkg</pkg-ref>
XML
[ "$HAVE_AU" = 1 ] && echo "    <pkg-ref id=\"studio.amanorsac.aether.au\" version=\"$VERSION\" onConclusion=\"none\">au.pkg</pkg-ref>"
cat <<XML
    <pkg-ref id="studio.amanorsac.aether.support" version="$VERSION" onConclusion="none">support.pkg</pkg-ref>
</installer-gui-script>
XML
} > "$DIST_XML"

# ---- build, sign if we hold the Installer identity ----
PKG="$DIST/AETHER-$VERSION-macOS.pkg"     # P28: <Product>-<version>-macOS.pkg
rm -f "$PKG"
PB=(productbuild --distribution "$DIST_XML" --package-path "$PKGS" --resources "$HERE/resources")
if [ -n "${INSTALLER_IDENTITY:-}" ]; then
    PB+=(--sign "$INSTALLER_IDENTITY" --timestamp)
else
    echo "note: INSTALLER_IDENTITY unset - building an UNSIGNED .pkg"
fi
"${PB[@]}" "$PKG"

# Fail here rather than uploading something malformed.
pkgutil --check-signature "$PKG" || true
pkgutil --payload-files "$PKG" >/dev/null
echo "Installer: $PKG"
echo "Size:      $(du -h "$PKG" | cut -f1)"
