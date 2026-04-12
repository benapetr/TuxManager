#!/bin/bash
################################################################################
# package-arch.sh - Build and package Tux Manager for Arch Linux from local repo
################################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
WORK_DIR="$PROJECT_ROOT/packaging/.arch-build"
STAGING_ROOT="$WORK_DIR/staging"
MAKEPKG_DIR="$WORK_DIR/makepkg"
NCPUS=$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
PKGREL="1"

usage() {
    echo "Usage: $0 [--version x.y.z]"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version)
            APP_VERSION="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

if [[ ${EUID:-$(id -u)} -eq 0 ]]; then
    echo "Error: do not run package-arch.sh as root."
    echo "Use a normal user account; makepkg will invoke fakeroot as needed."
    exit 1
fi

for cmd in makepkg qmake6 tar b2sum; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: required command not found: $cmd"
        exit 1
    fi
done

echo "============================="
echo "Building Tux Manager for Arch"
echo "============================="
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION-$PKGREL"
echo ""

SOURCE_DIR_NAME="TuxManager-${APP_VERSION}"
SOURCE_TARBALL="$WORK_DIR/${APP_NAME}-${APP_VERSION}.tar.gz"

rm -rf "$WORK_DIR"
mkdir -p "$OUTPUT_DIR" "$STAGING_ROOT/$SOURCE_DIR_NAME" "$MAKEPKG_DIR"

echo "Step 1: Preparing source tarball from local checkout..."
tar \
    --exclude-vcs \
    --exclude='./build' \
    --exclude='./packaging/output' \
    --exclude='./packaging/.arch-build' \
    -cf - \
    -C "$PROJECT_ROOT" . | tar -xf - -C "$STAGING_ROOT/$SOURCE_DIR_NAME"

tar -C "$STAGING_ROOT" -czf "$SOURCE_TARBALL" "$SOURCE_DIR_NAME"

echo ""
echo "Step 2: Patching PKGBUILD for local build..."
LOCAL_CHECKSUM="$(b2sum "$SOURCE_TARBALL" | awk '{print $1}')"
cp "$SCRIPT_DIR/arch/PKGBUILD" "$MAKEPKG_DIR/PKGBUILD"
sed -i "s/^pkgver=.*/pkgver=${APP_VERSION}/" "$MAKEPKG_DIR/PKGBUILD"
sed -i "s/^pkgrel=.*/pkgrel=${PKGREL}/" "$MAKEPKG_DIR/PKGBUILD"
sed -i "s|^source=.*|source=(\"${APP_NAME}-${APP_VERSION}.tar.gz::file://${SOURCE_TARBALL}\")|" "$MAKEPKG_DIR/PKGBUILD"
sed -i "s|^b2sums=.*|b2sums=('${LOCAL_CHECKSUM}')|" "$MAKEPKG_DIR/PKGBUILD"

echo ""
echo "Step 3: Building package with makepkg..."
(
    cd "$MAKEPKG_DIR"
    PKGDEST="$OUTPUT_DIR" makepkg --clean --cleanbuild --force
)

echo ""
echo "============================="
echo "Build complete!"
echo "============================="
echo "Package(s):"
find "$OUTPUT_DIR" -maxdepth 1 -type f -name "${APP_NAME}-${APP_VERSION}-${PKGREL}-*.pkg.tar.*" -print | sort
echo ""
echo "To install:"
echo "  sudo pacman -U $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${PKGREL}-*.pkg.tar.*"
echo ""
