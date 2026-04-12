#!/bin/bash
################################################################################
# render-arch-files.sh - Render Arch Linux packaging metadata from shared config
################################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

MODE="release"
PKGREL="1"
CHECKSUM_MODE="skip"
OUTPUT_DIR="$SCRIPT_DIR/arch"
LOCAL_SOURCE_TARBALL=""
GENERATE_SRCINFO="yes"
PACKAGE_NAME="$APP_NAME"

usage() {
    cat <<EOF
Usage:
  $0 [--version x.y.z] [--pkgrel N] [--checksum auto|skip|VALUE]
  $0 --mode local --source-tarball /abs/path/source.tar.gz [--version x.y.z] [--pkgrel N] [--package-name name] [--output-dir /abs/path] [--no-srcinfo]
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode)
            MODE="$2"
            shift 2
            ;;
        --version)
            APP_VERSION="$2"
            shift 2
            ;;
        --pkgrel)
            PKGREL="$2"
            shift 2
            ;;
        --package-name)
            PACKAGE_NAME="$2"
            shift 2
            ;;
        --checksum)
            CHECKSUM_MODE="$2"
            shift 2
            ;;
        --source-tarball)
            LOCAL_SOURCE_TARBALL="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --no-srcinfo)
            GENERATE_SRCINFO="no"
            shift
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

if [[ "$MODE" != "release" && "$MODE" != "local" ]]; then
    echo "Error: unsupported mode: $MODE"
    exit 1
fi

SOURCE_DIR_NAME="TuxManager-${APP_VERSION}"
mkdir -p "$OUTPUT_DIR"

if [[ "$MODE" == "release" ]]; then
    SOURCE_URL="${APP_HOMEPAGE_URL}/archive/refs/tags/v${APP_VERSION}.tar.gz"
    SOURCE_SPEC="${PACKAGE_NAME}-${APP_VERSION}.tar.gz::${SOURCE_URL}"

    case "$CHECKSUM_MODE" in
        auto)
            if ! command -v curl >/dev/null 2>&1; then
                echo "Error: curl is required for --checksum auto"
                exit 1
            fi
            CHECKSUM_VALUE="$(curl -fsSL "$SOURCE_URL" | b2sum | awk '{print $1}')"
            ;;
        skip)
            CHECKSUM_VALUE="SKIP"
            ;;
        *)
            CHECKSUM_VALUE="$CHECKSUM_MODE"
            ;;
    esac
else
    if [[ -z "$LOCAL_SOURCE_TARBALL" ]]; then
        echo "Error: --source-tarball is required in local mode"
        exit 1
    fi
    if [[ ! -f "$LOCAL_SOURCE_TARBALL" ]]; then
        echo "Error: source tarball not found: $LOCAL_SOURCE_TARBALL"
        exit 1
    fi

    LOCAL_SOURCE_TARBALL="$(cd "$(dirname "$LOCAL_SOURCE_TARBALL")" && pwd)/$(basename "$LOCAL_SOURCE_TARBALL")"
    SOURCE_SPEC="${PACKAGE_NAME}-${APP_VERSION}.tar.gz::file://${LOCAL_SOURCE_TARBALL}"
    CHECKSUM_VALUE="$(b2sum "$LOCAL_SOURCE_TARBALL" | awk '{print $1}')"
fi

cat > "$OUTPUT_DIR/PKGBUILD" <<EOF
pkgname=${PACKAGE_NAME}
pkgver=${APP_VERSION}
pkgrel=${PKGREL}
pkgdesc="${DESCRIPTION}"
arch=('x86_64')
url="${APP_HOMEPAGE_URL}"
license=('GPL-3.0-or-later')
depends=('qt6-base')
makedepends=('qt6-base')
options=('!debug')
source=("${SOURCE_SPEC}")
b2sums=('${CHECKSUM_VALUE}')

build() {
    cd "\${srcdir}/${SOURCE_DIR_NAME}"

    mkdir -p build
    cd build

    qmake6 ../src/TuxManager.pro
    make -j"\$(nproc)"
}

package() {
    cd "\${srcdir}/${SOURCE_DIR_NAME}"

    install -Dm755 "build/tux-manager" "\${pkgdir}/usr/bin/tux-manager"
    install -Dm644 README.md "\${pkgdir}/usr/share/doc/\${pkgname}/README.md"
    install -Dm644 LICENSE "\${pkgdir}/usr/share/licenses/\${pkgname}/LICENSE"
    install -Dm644 packaging/flatpak/io.github.benapetr.TuxManager.desktop \
        "\${pkgdir}/usr/share/applications/io.github.benapetr.TuxManager.desktop"
    install -Dm644 packaging/flatpak/io.github.benapetr.TuxManager.svg \
        "\${pkgdir}/usr/share/icons/hicolor/scalable/apps/io.github.benapetr.TuxManager.svg"
    install -Dm644 packaging/flatpak/io.github.benapetr.TuxManager.metainfo.xml \
        "\${pkgdir}/usr/share/metainfo/io.github.benapetr.TuxManager.metainfo.xml"
}
EOF

if [[ "$GENERATE_SRCINFO" == "yes" ]]; then
    if ! command -v makepkg >/dev/null 2>&1; then
        echo "Error: makepkg is required to generate .SRCINFO"
        exit 1
    fi
    (
        cd "$OUTPUT_DIR"
        makepkg --printsrcinfo > .SRCINFO
    )
fi
