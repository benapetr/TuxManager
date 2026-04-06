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
            version = "0.0.1";
            src = ./.;
            nativeBuildInputs = with pkgs.kdePackages; [ qmake wrapQtAppsHook ];
            buildInputs = with pkgs.kdePackages; [ qtbase ] ;
            configurePhase = "qmake6 $src/src";
            buildPhase = "make -j$NIX_BUILD_CORES";
            installPhase = "mkdir -p $out && cp tux-manager $out/tux-manager";

        };
      }
    );
}
