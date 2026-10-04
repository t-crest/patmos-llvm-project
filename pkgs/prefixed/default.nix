{
  pkgs,
  system,
  patmos-toolchain-src,
  patmos-toolchain-release,
  patmos-simulator,
}: let
  repoSrc = ../..;

  # Keep source hashing stable by excluding local build outputs.
  filteredRepoSrc = pkgs.lib.cleanSourceWith {
    src = repoSrc;
    filter = path: _type: let
      relPath = pkgs.lib.removePrefix "${toString repoSrc}/" (toString path);
      ignored =
        relPath
        == ".git"
        || pkgs.lib.hasPrefix ".git/" relPath
        || relPath == ".github"
        || pkgs.lib.hasPrefix ".github/" relPath
        || relPath == ".ci"
        || pkgs.lib.hasPrefix ".ci/" relPath
        || relPath == ".forgejo"
        || pkgs.lib.hasPrefix ".forgejo/" relPath
        || relPath == "build"
        || pkgs.lib.hasPrefix "build/" relPath
        || relPath == "build-compiler-rt"
        || pkgs.lib.hasPrefix "build-compiler-rt/" relPath
        || relPath == "build-newlib"
        || pkgs.lib.hasPrefix "build-newlib/" relPath
        || relPath == "result"
        || pkgs.lib.hasPrefix "result/" relPath
        || (builtins.match "[^/]+\\.md" relPath) != null
        || pkgs.lib.hasSuffix ".log" relPath;
    in
      !ignored;
  };

  # Keep default target centralized for all prefixed compiler frontends.
  patmosTriple = "patmos-unknown-unknown-elf";

  mkPrefixed = name: toolchainRoot:
    pkgs.stdenv.mkDerivation {
      inherit name;
      dontUnpack = true;
      nativeBuildInputs = [pkgs.makeWrapper];

      installPhase = ''
              mkdir -p "$out/bin"

              makeWrapper "${toolchainRoot}/bin/clang" "$out/bin/patmos-clang" \
                --set-default PATMOS_TRIPLE "${patmosTriple}" \
                --set-default PATMOS_SYSROOT "${toolchainRoot}/newlib-sysroot" \
                --set-default PASIM "${patmos-simulator}/bin/pasim" \
                --add-flags "--target=${patmosTriple}"

              makeWrapper "${toolchainRoot}/bin/clang++" "$out/bin/patmos-clang++" \
                --set-default PATMOS_TRIPLE "${patmosTriple}" \
                --set-default PATMOS_SYSROOT "${toolchainRoot}/newlib-sysroot" \
                --set-default PASIM "${patmos-simulator}/bin/pasim" \
                --add-flags "--target=${patmosTriple}"

              makeWrapper "${toolchainRoot}/bin/llc" "$out/bin/patmos-llc"
              makeWrapper "${toolchainRoot}/bin/opt" "$out/bin/patmos-opt"
              makeWrapper "${toolchainRoot}/bin/llvm-ar" "$out/bin/patmos-llvm-ar"
              makeWrapper "${toolchainRoot}/bin/llvm-ranlib" "$out/bin/patmos-llvm-ranlib"
              makeWrapper "${toolchainRoot}/bin/llvm-link" "$out/bin/patmos-llvm-link"
              makeWrapper "${toolchainRoot}/bin/llvm-config" "$out/bin/patmos-llvm-config"
              makeWrapper "${toolchainRoot}/bin/llvm-objdump" "$out/bin/patmos-llvm-objdump"
              if [ -x "${toolchainRoot}/bin/llvm-lit" ]; then
                makeWrapper "${toolchainRoot}/bin/llvm-lit" "$out/bin/patmos-llvm-lit"
              fi
              makeWrapper "${toolchainRoot}/bin/lld" "$out/bin/patmos-lld"
              makeWrapper "${patmos-simulator}/bin/pasim" "$out/bin/patmos-pasim"

              cat > "$out/bin/patmos-env" <<'EOF'
        #!/usr/bin/env sh
        # Print portable environment exports for shells that support eval.
        echo "export PATMOS_TRIPLE=${patmosTriple}"
        echo "export PATMOS_SYSROOT=${toolchainRoot}/newlib-sysroot"
        echo "export PASIM=${patmos-simulator}/bin/pasim"
        EOF
              chmod +x "$out/bin/patmos-env"
      '';
    };

  prefixedSrc = mkPrefixed "patmos-prefixed-src-${system}" patmos-toolchain-src;
  prefixedRelease = mkPrefixed "patmos-prefixed-release-${system}" patmos-toolchain-release;
in {
  packages = {
    patmos-prefixed-src = prefixedSrc;
    patmos-prefixed-release = prefixedRelease;
    patmos-prefixed = prefixedRelease;
  };

  # Smoke-test the packaged toolchain: compile a real program with
  # patmos-clang (driver, newlib sysroot, crt0, compiler-rt, lld) and run it
  # under pasim, checking the program's exit code comes back. Full log in $out.
  checks.patmos-package-tests = pkgs.stdenv.mkDerivation {
    name = "patmos-package-tests-${system}";
    dontUnpack = true;
    nativeBuildInputs = [prefixedRelease];

    # Without this, stdenv never runs checkPhase.
    doCheck = true;

    checkPhase = ''
      set -o pipefail
      export PASIM="${patmos-simulator}/bin/pasim"
      export PATH="${patmos-simulator}/bin:$PATH"
      cat > hello.c <<'EOF'
#include <stdio.h>

int main(void) {
  printf("hello patmos\n");
  return 42;
}
EOF
      echo "Compiling with patmos-clang..." | tee patmos-package-tests.log
      patmos-clang hello.c -o hello.elf 2>&1 | tee -a patmos-package-tests.log
      echo "Running under pasim (expecting exit code 42 from main)..." | tee -a patmos-package-tests.log
      set +e
      pasim hello.elf >pasim-run.log 2>&1
      status=$?
      set -e
      if [ "$status" -ne 42 ]; then
        echo "ERROR: pasim exited with $status, expected 42" | tee -a patmos-package-tests.log
        cat pasim-run.log | tee -a patmos-package-tests.log
        exit 2
      fi
      echo "exit code $status matches main's return value" | tee -a patmos-package-tests.log
      cat pasim-run.log >> patmos-package-tests.log || true
    '';

    installPhase = ''
      mkdir -p $out
      cp patmos-package-tests.log "$out/"
    '';
  };
}
