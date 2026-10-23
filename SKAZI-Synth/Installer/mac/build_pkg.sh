#!/bin/bash
# Builds the macOS installer: SKAZI_Setup_v<version>_mac.pkg
#
#   build_pkg.sh <artefacts dir> <version> [output dir]
#
# <artefacts dir> is the CMake SKAZI_artefacts/Release folder (contains VST3/, AU/, Standalone/).
# Optional signing (needs an Apple Developer account):
#   APP_SIGN_ID="Developer ID Application: Name (TEAMID)"
#   PKG_SIGN_ID="Developer ID Installer: Name (TEAMID)"
# Without them the binaries are ad-hoc signed so they load on Apple Silicon.
set -euo pipefail

ART="$1"
VER="$2"
OUT="${3:-dist}"
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
mkdir -p "$OUT"

stage () # <name> <source bundle> <install dir>
{
    mkdir -p "$WORK/$1/$3"
    cp -R "$2" "$WORK/$1/$3/"
    if [ -n "${APP_SIGN_ID:-}" ]; then
        codesign --force --deep --options runtime --timestamp --sign "$APP_SIGN_ID" "$WORK/$1/$3/$(basename "$2")"
    else
        codesign --force --deep --sign - "$WORK/$1/$3/$(basename "$2")"
    fi
    # Keep bundles at their install location (don't let Installer relocate them)
    pkgbuild --analyze --root "$WORK/$1" "$WORK/$1.plist"
    plutil -replace BundleIsRelocatable -bool NO "$WORK/$1.plist" || true
    pkgbuild --root "$WORK/$1" --component-plist "$WORK/$1.plist" \
        --identifier "com.skazi.synth.$1" --version "$VER" \
        --install-location / "$WORK/skazi_$1.pkg"
}

stage vst3 "$ART/VST3/SKAZI.vst3"           "Library/Audio/Plug-Ins/VST3"
stage au   "$ART/AU/SKAZI.component"        "Library/Audio/Plug-Ins/Components"
stage app  "$ART/Standalone/SKAZI.app"      "Applications"

sed "s/@VERSION@/$VER/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
mkdir -p "$WORK/resources"
cp "$HERE/welcome.html" "$WORK/resources/"

PKG="$OUT/SKAZI_Setup_v${VER}_mac.pkg"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" \
    --resources "$WORK/resources" ${PKG_SIGN_ID:+--sign "$PKG_SIGN_ID"} "$PKG"

echo "Built $PKG"
