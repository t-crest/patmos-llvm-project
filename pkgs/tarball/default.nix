{
  pkgs,
  system,
  patmos-toolchain,
  patmos-simulator,
  # Test gate: writing these store paths into the manifest references every
  # test derivation, so this tarball cannot be built unless every test
  # suite (LLVM, Clang, LLD, compiler-rt, packaged toolchain) passed.
  # On Darwin only compiler-rt and the packaged toolchain gate it; see
  # gateManifest.
  patmos-llvm-tests,
  patmos-clang-tests,
  patmos-lld-tests,
  patmos-compiler-rt-tests,
  patmos-package-tests,
}: let
  triple = "patmos-unknown-unknown-elf";
  isDarwin = pkgs.stdenv.isDarwin;
  isLinux = pkgs.stdenv.isLinux;

  hostTarName = {
    "x86_64-linux" = "patmos-llvm-x86_64-linux-gnu";
    "aarch64-linux" = "patmos-llvm-aarch64-linux-gnu";
    "x86_64-darwin" = "patmos-llvm-x86_64-apple-darwin";
    "aarch64-darwin" = "patmos-llvm-arm64-apple-darwin";
  }.${system} or (throw "patmos-tarball: unsupported system ${system}");

  # Standard FHS interpreter of the target host, not a nix store path, so
  # the binaries run on any (recent enough) non-Nix Linux. Host glibc must
  # be >= the glibc of this nixpkgs; everything else is bundled below.
  # Linux only; macOS has no ELF interpreter (see darwinFixup).
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

  # Linux fixup: make the binaries runnable on non-Nix Linux and bundle
  # every non-glibc dependency next to them; fail loudly if one cannot be
  # resolved so the tarball never ships broken. The original rpath (read
  # BEFORE patchelf replaces it) points at the store dirs of the exact
  # libraries the binary was linked against, so it is the primary
  # resolution source; bundleLibDirs is the fallback.
  linuxFixup = ''
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
  '';

  # Darwin fixup: nix binaries reference /nix/store dylibs by absolute path.
  # Rewrite every reference to @rpath, copy the dylibs next to the binaries,
  # chase their dependencies to a fixpoint, and re-sign; editing load
  # commands invalidates the code signature.
  darwinFixup = ''
    PENDING="root/bin"
    while [ -n "$PENDING" ]; do
      CURRENT="$PENDING"
      PENDING=""
      for f in $CURRENT; do
        [ -f "$f" ] || continue
        otool -L "$f" | tail -n +2 | awk '{print $1}' \
          | grep '^/nix/store/.*\.dylib$' > deps.txt || true
        while IFS= read -r ref; do
          name=$(basename "$ref")
          if [ ! -e "root/lib/$name" ]; then
            cp "$ref" root/lib/
            PENDING="$PENDING root/lib/$name"
          fi
          install_name_tool -change "$ref" "@rpath/$name" "$f"
        done < deps.txt
        install_name_tool -id "@rpath/$(basename "$f")" "$f" 2>/dev/null || true
        install_name_tool -add_rpath "@executable_path/../lib" "$f" 2>/dev/null || true
        install_name_tool -add_rpath "@loader_path/." "$f" 2>/dev/null || true
        codesign -f -s - "$f"
      done
    done
    rm -f deps.txt
  '';

  # Gate: on Linux the tarball requires every suite to have passed. The
  # full LLVM/Clang/LLD test trees do not fit the macOS CI runners, so on
  # Darwin the gate is the in-chain compiler-rt tests plus the packaged
  # toolchain smoke test.
  gateManifest =
    pkgs.lib.optionalString isLinux ''
    patmos-llvm-tests:        ${patmos-llvm-tests}
    patmos-clang-tests:       ${patmos-clang-tests}
    patmos-lld-tests:         ${patmos-lld-tests}
    '' + ''
    patmos-compiler-rt-tests: ${patmos-compiler-rt-tests}
    patmos-package-tests:     ${patmos-package-tests}
    '';

  hostRequirement =
    if isDarwin
    then "Requires macOS >= the deployment target of the building nixpkgs."
    else "Requires host glibc >= the glibc of the building nixpkgs.";
in {
  packages.patmos-tarball = pkgs.stdenv.mkDerivation {
    name = "patmos-tarball-${system}";
    dontUnpack = true;
    nativeBuildInputs =
      with pkgs;
        [gnutar]
        ++ pkgs.lib.optionals isDarwin [darwin.cctools darwin.sigtool]
        ++ pkgs.lib.optionals isLinux [patchelf];

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

      ${pkgs.lib.optionalString isLinux linuxFixup}
      ${pkgs.lib.optionalString isDarwin darwinFixup}

      # Gate manifest: each path is a build input of this derivation, so
      # this file cannot exist unless every test suite passed.
      cat > root/TESTS-PASSED.txt <<EOF
      Built by Nix; gated on every test suite passing:
      ${gateManifest}
      ${hostRequirement}
      EOF

      tar -czf "$out/${hostTarName}.tar.gz" -C root .
    '';
  };
}
