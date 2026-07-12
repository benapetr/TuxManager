#!/bin/bash
################################################################################
# package-suse.sh - Build and package Tux Manager for openSUSE
################################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

QT_BIN_PATH=""
RELEASE="1"

usage() {
    echo "Usage: $0 [--qt /path/to/qt/bin] [--version x.y.z]"
}

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

if [[ -n "$QT_BIN_PATH" ]]; then
    export PATH="$QT_BIN_PATH:$PATH"
fi

echo "=================================="
echo "Building Tux Manager for openSUSE"
echo "=================================="
echo "Version: $APP_VERSION-$RELEASE"
echo ""

if ! command -v rpm >/dev/null 2>&1 || ! command -v zypper >/dev/null 2>&1; then
    echo "Error: this script must be run on an openSUSE system with rpm and zypper."
    exit 1
fi

required_packages=(
    rpm-build
    git
    gzip
    pkgconf-pkg-config
    qt6-base-common-devel
    qt6-core-devel
    qt6-gui-devel
    qt6-widgets-devel
)
missing=()
for pkg in "${required_packages[@]}"; do
    if ! rpm -q "$pkg" >/dev/null 2>&1; then
        missing+=("$pkg")
    fi
done

if [[ ${#missing[@]} -gt 0 ]]; then
    echo "Missing build dependencies detected."
    echo ""
    echo "  sudo zypper install ${missing[*]}"
    echo ""
    exit 1
fi

if command -v qmake6 >/dev/null 2>&1; then
    QMAKE_PATH="$(command -v qmake6)"
elif command -v qmake >/dev/null 2>&1 && qmake -query QT_VERSION 2>/dev/null | grep -q '^6\.'; then
    QMAKE_PATH="$(command -v qmake)"
else
    echo "Error: Qt 6 qmake not found in PATH."
    echo "Install qt6-base-common-devel or use --qt to specify the Qt bin path."
    exit 1
fi

echo "Qt command: $QMAKE_PATH"
echo ""

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
RPM_TOPDIR="$(mktemp -d "${TMPDIR:-/tmp}/tux-manager-suse-XXXXXX")"
trap 'rm -rf "$RPM_TOPDIR"' EXIT

mkdir -p "$RPM_TOPDIR"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS} "$OUTPUT_DIR"

echo "Step 1: Preparing source archive..."

if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Error: $PROJECT_ROOT is not a git repository."
    exit 1
fi

SOURCE_DIR_NAME="${APP_NAME}-${APP_VERSION}"
SOURCE_TARBALL="$RPM_TOPDIR/SOURCES/$SOURCE_DIR_NAME.tar.gz"
git -C "$PROJECT_ROOT" archive --format=tar --prefix "$SOURCE_DIR_NAME/" HEAD | gzip -n > "$SOURCE_TARBALL"

echo ""
echo "Step 2: Creating openSUSE RPM spec file..."

SPEC_PATH="$RPM_TOPDIR/SPECS/$APP_NAME.spec"
cat > "$SPEC_PATH" <<EOF_SPEC
%global qmake_cmd $QMAKE_PATH
%global debug_package %{nil}

Name:           $APP_NAME
Version:        $APP_VERSION
Release:        $RELEASE
Summary:        $DESCRIPTION
License:        GPL-3.0-or-later
Group:          System/Monitoring
URL:            $APP_HOMEPAGE_URL
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(Qt6Widgets)

%description
Tux Manager is a Linux system monitor inspired by Windows Task Manager.

%prep
%autosetup

%build
mkdir -p release
pushd src
%{qmake_cmd} TuxManager.pro -o ../release/Makefile \
    QMAKE_CFLAGS="%{optflags}" \
    QMAKE_CXXFLAGS="%{optflags}" \
    QMAKE_LFLAGS="%{?build_ldflags} -Wl,--as-needed"
popd
%make_build -C release

%install
install -Dm755 release/tux-manager %{buildroot}%{_bindir}/$APP_NAME
install -Dm644 packaging/data/io.github.benapetr.TuxManager.desktop \
    %{buildroot}%{_datadir}/applications/io.github.benapetr.TuxManager.desktop
install -Dm644 src/tux_manager_icon.svg \
    %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/tux_manager_icon.svg
install -Dm644 packaging/data/io.github.benapetr.TuxManager.metainfo.xml \
    %{buildroot}%{_datadir}/metainfo/io.github.benapetr.TuxManager.metainfo.xml

%files
%license LICENSE
%doc README.md
%{_bindir}/$APP_NAME
%{_datadir}/applications/io.github.benapetr.TuxManager.desktop
%{_datadir}/icons/hicolor/scalable/apps/tux_manager_icon.svg
%{_datadir}/metainfo/io.github.benapetr.TuxManager.metainfo.xml

%changelog
EOF_SPEC

echo ""
echo "Step 3: Building RPM and source RPM..."

rpmbuild --define "_topdir $RPM_TOPDIR" -ba "$SPEC_PATH"

shopt -s nullglob
artifacts=(
    "$RPM_TOPDIR"/RPMS/*/"${APP_NAME}-${APP_VERSION}-${RELEASE}"*.rpm
    "$RPM_TOPDIR"/SRPMS/"${APP_NAME}-${APP_VERSION}-${RELEASE}"*.src.rpm
)
shopt -u nullglob

if [[ ${#artifacts[@]} -eq 0 ]]; then
    echo "Error: rpmbuild completed but no RPM artifacts were found."
    exit 1
fi

cp "${artifacts[@]}" "$OUTPUT_DIR/"

echo ""
echo "=================================="
echo "Build complete!"
echo "=================================="
echo "Package(s):"
for artifact in "${artifacts[@]}"; do
    echo "  $OUTPUT_DIR/$(basename "$artifact")"
done
echo ""
echo "To install:"
echo "  sudo zypper install $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}.$(rpm --eval '%{_arch}').rpm"
echo ""
