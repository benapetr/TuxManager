{
  description = "A Linux Task Manager alternative built with Qt6, inspired by the Windows Task Manager but designed to go further - providing deep visibility into system processes, performance metrics, users, and services.";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/25.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      nixpkgs,
      flake-utils,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        formatter = pkgs.nixfmt-rfc-style;

        packages.default =
          let
            desktopItem = pkgs.makeDesktopItem {
              type = "Application";
              name = "tux Manager";
              desktopName = "Tux Manager";
              comment = "Linux system monitor inspired by Windows Task Manager";
              exec = "tux-manager";
              icon = "tux-manager";
              categories = [ "System" "Monitor" ];
              terminal = false;
            };
            icon = ./res/tux_manager_icon.svg;
          in
          pkgs.stdenv.mkDerivation {
            pname = "tux-manager";
            version = "1.0.3";
            src = ./.;
            nativeBuildInputs = with pkgs.kdePackages; [ qmake wrapQtAppsHook ];
            buildInputs = with pkgs.kdePackages; [ qtbase ];
            configurePhase = "qmake6 $src/src";
            buildPhase = "make -j$NIX_BUILD_CORES";
            installPhase = ''
              mkdir -p $out/bin
              cp tux-manager $out/bin/tux-manager

              mkdir -p $out/share/icons/hicolor/scalable/apps/
              cp ${icon} $out/share/icons/hicolor/scalable/apps/tux-manager.svg

              mkdir -p $out/share/applications
              ln -s ${desktopItem}/share/applications/* $out/share/applications/
            '';
          };
      }
    );
}
