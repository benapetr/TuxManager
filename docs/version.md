# Version Update Checklist

Canonical/manual version definitions:

1. `src/globals.h`
   Application/CLI version shown by `--version` and used by `QApplication::setApplicationVersion`.
2. `packaging/config` (`APP_VERSION`)
   Source version used by all packaging scripts (`package-deb.sh`, `package-rpm.sh`, `package-flatpak.sh`, `package-arch.sh`).
3. `debian/changelog` (top entry version)
   Debian source package version/changelog entry.
4. `packaging/flatpak/io.github.benapetr.TuxManager.metainfo.xml` (`<release version="...">`)
   Flatpak/AppStream release metadata.
5. `flake.nix` (`version = "..."`)
   Nix package version (currently separate from packaging/config).

Derived/generated version locations (usually do not edit manually):

1. `packaging/package-deb.sh` derives `DEB_VERSION` from `APP_VERSION` (+ distro suffix).
2. `packaging/package-rpm.sh` uses `APP_VERSION` from `packaging/config` (or `--version`) and `RELEASE`.
3. `packaging/package-flatpak.sh` uses `APP_VERSION` from `packaging/config` (or `--version`) for bundle naming.
4. `packaging/package-arch.sh` generates `PKGBUILD` from `packaging/config` for local Arch package builds.
5. The AUR publish workflow generates `PKGBUILD` and `.SRCINFO` at publish time from `packaging/config`.
6. `debian/tux-manager/DEBIAN/control` and other files under `debian/tux-manager*` are build artifacts.
