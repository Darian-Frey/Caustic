#!/usr/bin/env bash
# build-appimage.sh — produce Caustic-x86_64.AppImage from a clean Release build.
#
# Requirements (must be on $PATH):
#   - cmake, a C/C++ toolchain, the X11/GL system headers your distro packages
#   - linuxdeploy (single-file binary from
#         https://github.com/linuxdeploy/linuxdeploy/releases)
#   - appimagetool   (single-file binary from
#         https://github.com/AppImage/appimagetool/releases)
#
# Usage:
#   ./packaging/appimage/build-appimage.sh
#
# Output:
#   ./Caustic-x86_64.AppImage at the repo root.
set -euo pipefail

# -- paths --------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-appimage"
APPDIR="$REPO_ROOT/Caustic.AppDir"
OUTPUT="$REPO_ROOT/Caustic-x86_64.AppImage"

# -- prerequisite check -------------------------------------------------------
require() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "error: $1 not found on PATH — see header comment for download links." >&2
        exit 1
    }
}
require cmake
require linuxdeploy
require appimagetool

# -- 1. Configure and build a Release binary ----------------------------------
echo "[1/4] Configuring + building Caustic (Release, no tests)…"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCAUSTIC_BUILD_APP=ON \
    >/dev/null
cmake --build "$BUILD_DIR" -j --target caustic

# -- 2. Lay out the AppDir ----------------------------------------------------
echo "[2/4] Laying out AppDir at $APPDIR …"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/caustic/presets"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/scalable/apps"

cp "$BUILD_DIR/app/caustic" "$APPDIR/usr/bin/caustic"
cp "$REPO_ROOT/presets/"*.json "$APPDIR/usr/share/caustic/presets/"

# Top-level desktop file + icon are what appimagetool reads to populate the
# metadata block of the final AppImage. Duplicates under /usr/share follow the
# Freedesktop layout so an installed AppImage still integrates cleanly.
cp "$SCRIPT_DIR/caustic.desktop" "$APPDIR/caustic.desktop"
cp "$SCRIPT_DIR/caustic.desktop" "$APPDIR/usr/share/applications/caustic.desktop"
cp "$SCRIPT_DIR/caustic.svg"     "$APPDIR/caustic.svg"
cp "$SCRIPT_DIR/caustic.svg"     "$APPDIR/usr/share/icons/hicolor/scalable/apps/caustic.svg"

install -m 0755 "$SCRIPT_DIR/AppRun" "$APPDIR/AppRun"

# -- 3. Pull in transitive shared-lib deps ------------------------------------
# linuxdeploy walks ldd on the binary and copies every non-system library into
# $APPDIR/usr/lib. It also patches RPATH and copies the icon into the standard
# hicolor tree if it isn't there yet.
echo "[3/4] Bundling shared-library dependencies with linuxdeploy…"
linuxdeploy \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/caustic" \
    --desktop-file "$APPDIR/usr/share/applications/caustic.desktop" \
    --icon-file "$APPDIR/caustic.svg"

# -- 4. Pack ------------------------------------------------------------------
echo "[4/4] Packing AppImage → $OUTPUT …"
ARCH="${ARCH:-x86_64}" appimagetool "$APPDIR" "$OUTPUT"

echo
echo "Done. Built: $OUTPUT"
echo "Size: $(du -h "$OUTPUT" | cut -f1)"
echo "Run with: chmod +x $OUTPUT && $OUTPUT"
