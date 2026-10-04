{
  pkgs,
  system,
  patmos-llvm,
  patmos-newlib,
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
        # Nix packaging infrastructure: changes here do not affect the
        # built toolchain and must not invalidate its source hash.
        || relPath == "pkgs"
        || pkgs.lib.hasPrefix "pkgs/" relPath
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

  commonBuildInputs = with pkgs;
    [
      cmake
      ninja
      git
      clang
      gcc
      binutils
      python3
      pkg-config
      libxml2
      gnutar
      wget
      gnumake
    ]
    ++ pkgs.lib.optional pkgs.stdenv.isDarwin pkgs.darwin.cctools;
in rec {
  packages.patmos-compiler-rt = pkgs.stdenv.mkDerivation {
    name = "patmos-compiler-rt-${system}";
    src = filteredRepoSrc;
    buildInputs = [patmos-llvm patmos-newlib patmos-simulator];
    nativeBuildInputs = commonBuildInputs ++ [patmos-simulator];

    # patmos-newlib is Autotools-based; prevent the cmake hook from taking over.
    dontUseCmakeConfigure = true;
    # Keep the vendored config.sub: it accepts patmos-unknown-unknown-elf,
    # while the newer generic replacement from nixpkgs rejects that target.
    dontUpdateAutotoolsGnuConfigScripts = true;

    # Whoever fetches this package gets the unit tests built and run; disable
    # with `.overrideAttrs (finalAttrs: {doCheck = false;})`.
    doCheck = true;

    buildPhase = ''
      runHook preBuild
      mkdir -p build-compiler-rt
      cd build-compiler-rt
      cmake ../compiler-rt \
        -DCMAKE_INSTALL_PREFIX="$out" \
        -DCMAKE_TOOLCHAIN_FILE=../compiler-rt/cmake/patmos-clang-toolchain.cmake \
        -DCMAKE_C_COMPILER="${patmos-llvm}/bin/clang" \
        -DCMAKE_CXX_COMPILER="${patmos-llvm}/bin/clang++" \
        -DCOMPILER_RT_TEST_COMPILER="${patmos-llvm}/bin/clang" \
        -DLLVM_TOOLS_BINARY_DIR="${patmos-llvm}/bin" \
        -DLLVM_TOOLS_DIR="${patmos-llvm}/bin" \
        -DLLVM_CMAKE_DIR="${patmos-llvm}/lib/cmake/llvm" \
        -DLIBXML2_INCLUDE_DIR="${pkgs.lib.getDev pkgs.libxml2}/include/libxml2" \
        -DLIBXML2_LIBRARY="${pkgs.lib.getLib pkgs.libxml2}/lib/libxml2.${pkgs.stdenv.hostPlatform.extensions.sharedLibrary}" \
        -DCOMPILER_RT_INCLUDE_TESTS=ON \
        -DCOMPILER_RT_TEST_STANDALONE_BUILD_LIBS=OFF
      # Full send: every host core (see pkgs/llvm/default.nix).
      make -j"$(nproc)"
      runHook postBuild
    '';

    installPhase = ''
      runHook preInstall
      cd "$NIX_BUILD_TOP/$sourceRoot/build-compiler-rt"
      make install
      cp ../patmos-compiler-rt-tests.log "$out/"
      runHook postInstall
    '';

    # README-Maintainers sequence, from the build-compiler-rt folder:
    # ./bin/llvm-lit -v test/builtins/Unit/patmos
    # checkPhase runs with the working directory left by buildPhase.
    # A failing test fails the derivation; the full lit output is kept in
    # $out/patmos-compiler-rt-tests.log.
    checkPhase = ''
      export PASIM="${patmos-simulator}/bin/pasim"
      export PATH="${patmos-simulator}/bin:$PATH"
      set -o pipefail
      if [ ! -f bin/llvm-lit ]; then
        echo "ERROR: compiler-rt build did not generate bin/llvm-lit" >&2
        exit 2
      fi
      # --param mirrors the CI invocation: lit.local.cfg needs llvm_tools_dir
      # to find llc/ld.lld for the patmos unit tests.
      ./bin/llvm-lit -v --param=llvm_tools_dir="${patmos-llvm}/bin" test/builtins/Unit/patmos 2>&1 | tee ../patmos-compiler-rt-tests.log
      if ! grep -q "^PASS:" ../patmos-compiler-rt-tests.log; then
        echo "ERROR: no tests reported PASS in patmos-compiler-rt-tests.log" >&2
        exit 2
      fi
    '';
  };

  # Alias so `nix flake check` runs the compiler-rt leg; the package itself
  # already tests by default (doCheck = true).
  checks.patmos-compiler-rt-tests = packages.patmos-compiler-rt;
}
