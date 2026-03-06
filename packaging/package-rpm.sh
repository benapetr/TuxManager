#!/bin/bash
################################################################################
# package-rpm.sh - Build and package Tux Manager for Fedora/RHEL/CentOS
################################################################################

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/config"

QT_BIN_PATH=""
NCPUS=$(nproc)
RELEASE="1"

while [[ $# -gt 0 ]]; do
    case $1 in
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

echo "==============================="
echo "Building Tux Manager for RPM"
echo "==============================="
echo "CPUs: $NCPUS"
echo "Version: $APP_VERSION-$RELEASE"
echo ""

pkg_available() {
    local pkg="$1"
    if command -v dnf >/dev/null 2>&1; then
        dnf -q list --available "$pkg" >/dev/null 2>&1
        return $?
    fi
    if command -v yum >/dev/null 2>&1; then
        yum -q list available "$pkg" >/dev/null 2>&1
        return $?
    fi
    return 1
}

QT_MAJOR=0
if command -v qmake6 >/dev/null 2>&1; then
    QT_MAJOR=6
elif command -v qmake-qt5 >/dev/null 2>&1; then
    QT_MAJOR=5
elif pkg_available qt6-qtbase-devel; then
    QT_MAJOR=6
elif pkg_available qt5-qtbase-devel; then
    QT_MAJOR=5
fi

if command -v rpm >/dev/null 2>&1; then
    missing=()

    require_pkg() {
        local pkg="$1"
        if ! rpm -q "$pkg" >/dev/null 2>&1; then
            missing+=("$pkg")
        fi
    }

    require_pkg rpm-build
    require_pkg rsync
    require_pkg git
    require_pkg pkgconf-pkg-config

    if [ "$QT_MAJOR" -eq 5 ]; then
        require_pkg qt5-qtbase-devel
    else
        require_pkg qt6-qtbase-devel
    fi

    if [ ${#missing[@]} -gt 0 ]; then
        echo "Missing build dependencies detected."
        echo ""
        if command -v dnf >/dev/null 2>&1; then
            echo "  sudo dnf install ${missing[*]}"
        elif command -v yum >/dev/null 2>&1; then
            echo "  sudo yum install ${missing[*]}"
        else
            for pkg in "${missing[@]}"; do
                echo "  - $pkg"
            done
        fi
        echo ""
        exit 1
    fi
fi

if [ -z "$QT_BIN_PATH" ]; then
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake-qt5 >/dev/null 2>&1; then
        QMAKE_CMD="qmake-qt5"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    else
        echo "Error: qmake not found in PATH."
        exit 1
    fi
else
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE_CMD="qmake6"
    elif command -v qmake >/dev/null 2>&1; then
        QMAKE_CMD="qmake"
    elif command -v qmake-qt5 >/dev/null 2>&1; then
        QMAKE_CMD="qmake-qt5"
    else
        echo "Error: qmake not found in specified Qt path: $QT_BIN_PATH"
        exit 1
    fi
fi

QMAKE_PATH="$(command -v "$QMAKE_CMD")"

echo "Qt command: $QMAKE_CMD ($(dirname "$QMAKE_PATH"))"
if [ "$QT_MAJOR" -eq 5 ]; then
    echo "Qt major: 5"
elif [ "$QT_MAJOR" -eq 6 ]; then
    echo "Qt major: 6"
fi
echo ""

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

RPM_TOPDIR="$HOME/rpmbuild"
if command -v rpmdev-setuptree >/dev/null 2>&1; then
    rpmdev-setuptree >/dev/null 2>&1 || true
else
    mkdir -p "$RPM_TOPDIR"
fi
TEMP_DIR=$(mktemp -d -p "$RPM_TOPDIR" tux-manager-rpm-XXXXXX)
trap "rm -rf $TEMP_DIR" EXIT

echo "Step 1: Preparing source archive..."

RPM_BUILD_ROOT="$RPM_TOPDIR"
mkdir -p "$RPM_BUILD_ROOT"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

SOURCE_DIR_NAME="${APP_NAME}-${APP_VERSION}"
SOURCE_TARBALL="$RPM_BUILD_ROOT/SOURCES/$SOURCE_DIR_NAME.tar.gz"

if ! command -v git >/dev/null 2>&1; then
    echo "Error: git not found."
    exit 1
fi
if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Error: $PROJECT_ROOT is not a git repository."
    exit 1
fi

git -C "$PROJECT_ROOT" archive --format=tar --prefix "$SOURCE_DIR_NAME/" HEAD | gzip -n > "$SOURCE_TARBALL"

echo ""
echo "Step 2: Creating RPM spec file..."

SPEC_PATH="$RPM_BUILD_ROOT/SPECS/$APP_NAME.spec"
cat > "$SPEC_PATH" <<EOF_SPEC
%global qmake_cmd $QMAKE_PATH
%global debug_package %{nil}

Name:           $APP_NAME
Version:        $APP_VERSION
Release:        $RELEASE%{?dist}
Summary:        $DESCRIPTION

License:        GPL-3.0-or-later
URL:            https://github.com/benapetr/TuxManager
Source0:        %{name}-%{version}.tar.gz

%if 0%{?qt_major} == 5
BuildRequires:  qt5-qtbase-devel
%else
BuildRequires:  qt6-qtbase-devel
%endif
BuildRequires:  pkgconf-pkg-config

%description
Tux Manager is a Linux system monitor inspired by Windows Task Manager.

%prep
%autosetup

%build
mkdir -p release
pushd src
%{qmake_cmd} TuxManager.pro -o ../release/Makefile
popd
%make_build -C release

%install
rm -rf %{buildroot}
install -Dm755 release/tux-manager %{buildroot}%{_bindir}/$APP_NAME
install -Dm644 src/README.md %{buildroot}%{_docdir}/$APP_NAME/README.md
install -Dm644 LICENSE %{buildroot}%{_docdir}/$APP_NAME/LICENSE

install -d %{buildroot}%{_datadir}/applications
cat <<'DESKTOP' > %{buildroot}%{_datadir}/applications/$APP_NAME.desktop
[Desktop Entry]
Type=Application
Name=Tux Manager
Comment=Linux system monitor inspired by Windows Task Manager
Exec=$APP_NAME
Icon=/usr/share/pixmaps/$APP_NAME.ico
Categories=System;Monitor;
Terminal=false
DESKTOP

install -Dm644 src/linux_task_manager_256.ico %{buildroot}%{_datadir}/pixmaps/$APP_NAME.ico

%files
%license %{_docdir}/$APP_NAME/LICENSE
%doc %{_docdir}/$APP_NAME/README.md
%{_bindir}/$APP_NAME
%{_datadir}/applications/$APP_NAME.desktop
%{_datadir}/pixmaps/$APP_NAME.ico

%changelog
* $(date '+%a %b %d %Y') $MAINTAINER - $APP_VERSION-$RELEASE
- Automated package build
EOF_SPEC

echo ""
echo "Step 3: Building RPM and SRPM..."

rpmbuild --define "_topdir $RPM_BUILD_ROOT" \
         --define "qt_major $QT_MAJOR" \
         -ba "$SPEC_PATH"

OUTPUT_DIR="$PROJECT_ROOT/packaging/output"
mkdir -p "$OUTPUT_DIR"
cp "$RPM_BUILD_ROOT/RPMS/x86_64/${APP_NAME}-${APP_VERSION}-${RELEASE}"*.rpm "$OUTPUT_DIR/"
cp "$RPM_BUILD_ROOT/SRPMS/${APP_NAME}-${APP_VERSION}-${RELEASE}"*.src.rpm "$OUTPUT_DIR/"

echo ""
echo "==============================="
echo "Build complete!"
echo "==============================="
echo "Binary RPM: $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}*.rpm"
echo "Source RPM: $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}*.src.rpm"
echo ""
echo "To install:"
echo "  sudo dnf install $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}*.rpm"
echo "  # or"
echo "  sudo rpm -ivh $OUTPUT_DIR/${APP_NAME}-${APP_VERSION}-${RELEASE}*.rpm"
echo ""
