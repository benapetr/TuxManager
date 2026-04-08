# Tux Manager - Packaging

This directory contains Linux packaging scripts for Tux Manager.

## Supported targets

- Debian/Ubuntu (`.deb`) via `package-deb.sh`
- Fedora/RHEL/Alma/Rocky (`.rpm` + `.src.rpm`) via `package-rpm.sh`
- AppImage (`.AppImage`) via `package-appimage.sh`

## Unsupported targets
- Flatpak (`.flatpak`) via `package-flatpak.sh` - runs in container and doesn't allow access to host's /proc

## Dependencies

### Debian/Ubuntu

Required build tools/packages:

```bash
sudo apt-get install build-essential debhelper dpkg-dev pkg-config qt6-base-dev
```

Notes:
- The script accepts either `qmake6` or `qmake`.
- Qt5 can also work if your distro provides it (`qtbase5-dev`).

### Fedora/RHEL family

Required build tools/packages:

```bash
sudo dnf install rpm-build rsync git pkgconf-pkg-config qt6-qtbase-devel
```

Notes:
- The script can also build with Qt5 if only that is available (`qt5-qtbase-devel`).
- The script uses `rpmbuild` and creates both binary RPM and source RPM.

### Flatpak

NOTE: flatpak doesn't work right now - the resulting app runs in isolation and doesn't have access to /proc so you can't use it to full extent

Required tools/runtime:

```bash
sudo apt-get install flatpak flatpak-builder
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install flathub org.kde.Platform//6.7 org.kde.Sdk//6.7
```

Notes:
- The script validates that `flatpak`, `flatpak-builder`, `flathub`, and required runtime/sdk refs are present before building.
- The resulting bundle is written to `packaging/output/`.

### AppImage

Required tools/packages:

* https://github.com/linuxdeploy/linuxdeploy
* https://github.com/linuxdeploy/linuxdeploy-plugin-qt
* https://github.com/linuxdeploy/linuxdeploy-plugin-appimage

Download released appimages from all 3, put them into some directory and create symlinks with bare names, then add this folder to PATH variable

```bash
sudo apt-get install build-essential pkg-config qt6-base-dev
linuxdeploy --version
linuxdeploy-plugin-qt --help
linuxdeploy-plugin-appimage --help
```

Notes:
- `linuxdeploy`, `linuxdeploy-plugin-qt`, and `linuxdeploy-plugin-appimage` must be available in `PATH`.
- The script reuses the desktop/icon metadata from `packaging/flatpak/`.
- The resulting AppImage is written to `packaging/output/`.

## Usage

From repo root:

### Build DEB

```bash
cd packaging
./package-deb.sh
```

Optional:

```bash
./package-deb.sh --qt /path/to/qt/bin
./package-deb.sh --version 1.2.3
```

Output (in `packaging/output/`):
- `tux-manager_<version>_<arch>.deb`
- optionally `tux-manager-dbgsym_<version>_<arch>.deb`

### Build RPM

```bash
cd packaging
./package-rpm.sh
```

Optional:

```bash
./package-rpm.sh --qt /path/to/qt/bin
./package-rpm.sh --version 1.2.3
```

Output (in `packaging/output/`):
- `tux-manager-<version>-1.*.rpm`
- `tux-manager-<version>-1.*.src.rpm`

### Build Flatpak

```bash
cd packaging
./package-flatpak.sh
```

Optional:

```bash
./package-flatpak.sh --version 1.2.3
./package-flatpak.sh --branch beta
```

Output (in `packaging/output/`):
- `tux-manager-<version>-<arch>.flatpak`

### Build AppImage

```bash
cd packaging
./package-appimage.sh
```

Optional:

```bash
./package-appimage.sh --qt /path/to/qt/bin
./package-appimage.sh --version 1.2.3
```

Output (in `packaging/output/`):
- `tux-manager-<version>-<arch>.AppImage`

## Install

### Debian/Ubuntu

```bash
sudo dpkg -i packaging/output/tux-manager_*.deb
sudo apt-get install -f
```

### Fedora/RHEL family

```bash
sudo dnf install packaging/output/tux-manager-*.rpm
```

### Flatpak

```bash
flatpak install --user packaging/output/tux-manager-*.flatpak
flatpak run io.github.benapetr.TuxManager
```

### AppImage

```bash
chmod +x packaging/output/tux-manager-*.AppImage
./packaging/output/tux-manager-*.AppImage
```
