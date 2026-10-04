{
  pkgs,
  system,
  patmos-toolchain,
  patmos-simulator,
  # Test gate: writing these store paths into the manifest references every
  # test derivation, so this tarball cannot be built unless every test
  # suite (LLVM, Clang, LLD, compiler-rt, packaged toolchain) passed.
  patmos-llvm-tests,
  patmos-clang-tests,
  patmos-lld-tests,
  patmos-compiler-rt-tests,
  patmos-package-tests,
}: let
  triple = "patmos-unknown-unknown-elf";

  hostTarName = {
    "x86_64-linux" = "patmos-llvm-x86_64-linux-gnu";
    "aarch64-linux" = "patmos-llvm-aarch64-linux-gnu";
  }.${system} or (throw "patmos-tarball: unsupported system ${system}");

  # Standard FHS interpreter of the target host, not a nix store path, so the
  # binaries run on any (recent enough) non-Nix Linux. Host glibc must be
  # >= the glibc of this nixpkgs; everything else is bundled below.
  interpreter = {
    "x86_64-linux" = "/lib64/ld-linux-x86-64.so.2";
    "aarch64-linux" = "/lib/ld-linux-aarch64.so.1";
  }.${system};

  # Directories to resolve bundled (non-glibc) runtime libraries from.
  bundleLibDirs = with pkgs; [
    stdenv.cc.cc.lib # libstdc++.so.6, libgcc_s.so.1
    elfutils # libelf.so.1 (pasim)
    zlib # libz.so.1 (llvm)
    libxml2 # libxml2.so.2 (llvm)
  ];
in {
  packages.patmos-tarball = pkgs.stdenv.mkDerivation {
    name = "patmos-tarball-${system}";
    dontUnpack = true;
    nativeBuildInputs = with pkgs; [patchelf gnutar];

    installPhase = ''
      set -euo pipefail
      mkdir -p root/bin root/${triple} root/lib $out

      # Binary set mirroring the CI PatmosPackage, plus patmos-pasim so the
      # tarball can compile, run, and drive platin without extra downloads.
      # patmos-clang is the raw clang binary: the driver locates the runtime
      # at <bin>/../${triple} by itself (default target triple is patmos).
      cp ${patmos-toolchain}/bin/clang root/bin/patmos-clang
      cp ${patmos-toolchain}/bin/clang++ root/bin/patmos-clang++
      cp ${patmos-toolchain}/bin/llc root/bin/patmos-llc
      cp ${patmos-toolchain}/bin/opt root/bin/patmos-opt
      cp ${patmos-toolchain}/bin/lld root/bin/patmos-lld
      cp ${patmos-toolchain}/bin/llvm-link root/bin/patmos-llvm-link
      cp ${patmos-toolchain}/bin/llvm-config root/bin/patmos-llvm-config
      cp ${patmos-toolchain}/bin/llvm-objdump root/bin/patmos-llvm-objdump
      cp ${patmos-simulator}/bin/pasim root/bin/patmos-pasim

      VER=$(${patmos-toolchain}/bin/clang --version | sed -n 's/.*version \([0-9]*\).*/\1/p' | head -1)
      ln -s patmos-clang "root/bin/patmos-clang-$VER"
      ln -s patmos-lld root/bin/patmos-ld.lld

      # Runtime libs, includes, and clang resource headers
      cp -r ${patmos-toolchain}/${triple}/. root/${triple}/
      cp -r ${patmos-toolchain}/lib/clang root/lib/clang

      chmod -R u+w root

      # Make the binaries runnable on non-Nix Linux and bundle every
      # non-glibc dependency next to them; fail loudly if one cannot be
      # resolved so the tarball never ships broken. The original rpath
      # (read BEFORE patchelf replaces it) points at the store dirs of the
      # exact libraries the binary was linked against, so it is the
      # primary resolution source; bundleLibDirs is the fallback.
      for bin in root/bin/*; do
        if [ -L "$bin" ]; then continue; fi
        ORIG_RPATH=$(patchelf --print-rpath "$bin")
        patchelf --set-interpreter "${interpreter}" "$bin"
        patchelf --set-rpath '$ORIGIN/../lib' "$bin"
        for need in $(patchelf --print-needed "$bin"); do
          case "$need" in
            libc.so*|libm.so*|libpthread.so*|libdl.so*|librt.so*|ld-linux*)
              continue # provided by the host glibc
              ;;
          esac
          if [ -e "root/lib/$need" ]; then continue; fi
          FOUND=""
          for dir in $(echo "$ORIG_RPATH" | tr ':' ' ') \
                     ${toString (map (p: "${p}/lib") bundleLibDirs)}; do
            if [ -e "$dir/$need" ]; then
              cp "$dir/$need" root/lib/
              FOUND=1
              break
            fi
          done
          if [ -z "$FOUND" ]; then
            echo "ERROR: cannot resolve $need needed by $bin" >&2
            exit 2
          fi
        done
      done

      # Gate manifest: each path is a build input of this derivation, so
      # this file cannot exist unless every test suite passed.
      cat > root/TESTS-PASSED.txt <<EOF
      Built by Nix; gated on every test suite passing:
        patmos-llvm-tests:        ${patmos-llvm-tests}
        patmos-clang-tests:       ${patmos-clang-tests}
        patmos-lld-tests:         ${patmos-lld-tests}
        patmos-compiler-rt-tests: ${patmos-compiler-rt-tests}
        patmos-package-tests:     ${patmos-package-tests}
      Requires host glibc >= the glibc of the building nixpkgs.
      EOF

      tar -czf "$out/${hostTarName}.tar.gz" -C root .
    '';
  };
}
