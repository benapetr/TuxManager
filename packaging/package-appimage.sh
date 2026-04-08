#!/bin/bash
################################################################################
# package-appimage.sh - Build and package Tux Manager as an AppImage
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
BUILD_DIR="$PROJECT_ROOT/packaging/.appimage-build"
APPDIR="$PROJECT_ROOT/packaging/.AppDir"
DESKTOP_FILE_SRC="$PROJECT_ROOT/packaging/flatpak/io.github.benapetr.TuxManager.desktop"
ICON_FILE_SRC="$PROJECT_ROOT/packaging/flatpak/io.github.benapetr.TuxManager.svg"
METADATA_FILE_SRC="$PROJECT_ROOT/packaging/flatpak/io.github.benapetr.TuxManager.metainfo.xml"

QT_BIN_PATH=""
NCPUS=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt)
            QT_BIN_PATH="$2"
            shift 2
            ;;
        --version)
            APP_VERSION="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--qt /path/to/qt/bin] [--version x.y.z]"
            exit 1
            ;;
    esac
done

if [ -n "$QT_BIN_PATH" ]; then
    export PATH="$QT_BIN_PATH:$PATH"
fi

echo "================================="
echo "Building Tux Manager AppImage"
echo "================================="
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION"
echo ""

if [ -z "$QT_BIN_PATH" ]; then
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in PATH."
        echo "Install Qt development tools or use --qt to specify Qt bin path."
        exit 1
    fi
else
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in specified Qt path: $QT_BIN_PATH"
        exit 1
    fi
fi

if ! command -v linuxdeploy >/dev/null 2>&1; then
    echo "Error: linuxdeploy not found in PATH."
    echo "Install linuxdeploy and re-run."
    exit 1
fi

if ! command -v linuxdeploy-plugin-qt >/dev/null 2>&1; then
    echo "Error: linuxdeploy-plugin-qt not found in PATH."
    echo "Install linuxdeploy-plugin-qt and re-run."
    exit 1
fi

if ! command -v linuxdeploy-plugin-appimage >/dev/null 2>&1; then
    echo "Error: linuxdeploy-plugin-appimage not found in PATH."
    echo "Install linuxdeploy-plugin-appimage and re-run."
    exit 1
fi

if [ ! -f "$DESKTOP_FILE_SRC" ]; then
    echo "Error: Desktop file not found: $DESKTOP_FILE_SRC"
    exit 1
fi

if [ ! -f "$ICON_FILE_SRC" ]; then
    echo "Error: Icon file not found: $ICON_FILE_SRC"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
rm -rf "$BUILD_DIR" "$APPDIR"
mkdir -p "$BUILD_DIR" \
         "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/scalable/apps" \
         "$APPDIR/usr/share/metainfo"

echo "Step 1: Building application..."
cd "$BUILD_DIR"
"$QMAKE_CMD" "$PROJECT_ROOT/src/TuxManager.pro"
make -j"$NCPUS"

echo ""
echo "Step 2: Creating AppDir..."
install -Dm755 "$BUILD_DIR/$APP_NAME" "$APPDIR/usr/bin/$APP_NAME"
install -Dm644 "$DESKTOP_FILE_SRC" "$APPDIR/usr/share/applications/io.github.benapetr.TuxManager.desktop"
install -Dm644 "$ICON_FILE_SRC" "$APPDIR/usr/share/icons/hicolor/scalable/apps/io.github.benapetr.TuxManager.svg"
if [ -f "$METADATA_FILE_SRC" ]; then
    install -Dm644 "$METADATA_FILE_SRC" "$APPDIR/usr/share/metainfo/io.github.benapetr.TuxManager.metainfo.xml"
fi

ln -sf usr/share/applications/io.github.benapetr.TuxManager.desktop "$APPDIR/io.github.benapetr.TuxManager.desktop"
ln -sf usr/share/icons/hicolor/scalable/apps/io.github.benapetr.TuxManager.svg "$APPDIR/io.github.benapetr.TuxManager.svg"

echo ""
echo "Step 3: Bundling AppImage..."
cd "$PROJECT_ROOT"
rm -f "$PROJECT_ROOT"/*.AppImage
export QMAKE
QMAKE="$(command -v "$QMAKE_CMD")"

linuxdeploy \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/io.github.benapetr.TuxManager.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/io.github.benapetr.TuxManager.svg" \
    --plugin qt \
    --output appimage

mapfile -t APPIMAGE_FILES < <(find "$PROJECT_ROOT" -maxdepth 1 -type f -name "*.AppImage" | sort) || true
if [ ${#APPIMAGE_FILES[@]} -eq 0 ]; then
    echo "Error: linuxdeploy did not produce an AppImage."
    exit 1
fi

ARCH="$(uname -m)"
OUTPUT_PATH="$OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${ARCH}.AppImage"
mv "${APPIMAGE_FILES[0]}" "$OUTPUT_PATH"

echo ""
echo "================================="
echo "Build complete!"
echo "================================="
echo "AppImage: $OUTPUT_PATH"
echo ""
echo "To run:"
echo "  chmod +x $OUTPUT_PATH"
echo "  $OUTPUT_PATH"
echo ""
