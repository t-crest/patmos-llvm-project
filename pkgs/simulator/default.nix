{
  pkgs,
  system,
}: {
  packages.patmos-simulator = pkgs.stdenv.mkDerivation {
    name = "patmos-simulator-${system}";
    src = pkgs.fetchurl {
      url =
        if system == "x86_64-darwin"
        then "https://github.com/t-crest/patmos-simulator/releases/latest/download/patmos-simulator-x86_64-apple-darwin.tar.gz"
        else if system == "aarch64-darwin"
        then "https://github.com/t-crest/patmos-simulator/releases/download/1.0.8/patmos-simulator-arm64-apple-darwin.tar.gz"
        else "https://github.com/t-crest/patmos-simulator/releases/latest/download/patmos-simulator-x86_64-linux-gnu.tar.gz";
      sha256 =
        {
          "x86_64-darwin" = "sha256-HBK/5tJIl6ve8iuGIqzFCTszB4FRQtUuQowYnCpofFg=";
          "aarch64-darwin" = "sha256-6WaKA6JVRP9BDA6aSFFeghOMRMEFZ29bSIdlCWN4y/A=";
          "x86_64-linux" = "sha256-kxMr28pI7iHriGeghlRH+m8suGjPMavxJHxmkcPed2U=";
        }.${
          system
        } or (throw "Unsupported system: ${system}");
    };
    # The release tarball ships FHS binaries. Patch the ELF interpreter and
    # library search paths so the simulator also runs on non-FHS systems
    # (e.g. NixOS build sandboxes), not just on glibc distros and macOS.
    nativeBuildInputs = pkgs.lib.optional pkgs.stdenv.isLinux pkgs.autoPatchelfHook;
    # pkgs.libelf is GNU libelf (SONAME libelf.so.0); the prebuilt simulator
    # needs elfutils' libelf.so.1.
    buildInputs = pkgs.lib.optionals pkgs.stdenv.isLinux [pkgs.gcc.cc.lib pkgs.glibc pkgs.elfutils];

    buildPhase = ''
      mkdir -p $out/bin
      tar -xzf $src -C $out
      chmod +x $out/bin/pasim
    '';
    installPhase = "cp -r $out/bin/* $out/";
  };
}
