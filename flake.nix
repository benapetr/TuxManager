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

        packages.default = pkgs.stdenv.mkDerivation {
            pname = "tux-manager";
            version = "1.0.4";
            src = ./.;
            
            nativeBuildInputs = with pkgs.kdePackages; [ qmake wrapQtAppsHook ];
            buildInputs = with pkgs.kdePackages; [ qtbase ];

            configurePhase = "qmake6 $src/src";
            buildPhase = "make -j$NIX_BUILD_CORES";

            installPhase = ''
              mkdir -p $out/bin
              cp tux-manager $out/bin/tux-manager

              mkdir -p $out/share/icons/hicolor/scalable/apps/
              cp ./src/tux_manager_icon.svg $out/share/icons/hicolor/scalable/apps/tux_manager_icon.svg

              mkdir -p $out/share/applications
              cp ./packaging/data/io.github.benapetr.TuxManager.desktop $out/share/applications/io.github.benapetr.TuxManager.desktop
            '';

            postFixup = ''
              # fixes issue where nvml isn't found
              wrapProgram $out/bin/tux-manager --prefix LD_LIBRARY_PATH = /run/opengl-driver/lib
            '';
          };
      }
    );
}
