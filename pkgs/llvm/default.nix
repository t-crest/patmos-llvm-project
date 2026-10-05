{
  pkgs,
  system,
  simulator,
}: let
  repoSrc = ../..;

  # Avoid rebuilding LLVM when local build outputs/logs change.
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
      ccache
      lld # Keep lld in buildInputs
    ]
    ++ pkgs.lib.optional (system == "x86_64-linux") mold
    ++ pkgs.lib.optional pkgs.stdenv.isDarwin pkgs.darwin.cctools;

  ccacheConfig = ''
    mkdir -p $CCACHE_DIR
    export PATH="${pkgs.ccache}/bin:$PATH"
    ccache --show-stats || true
    ccache --zero-stats || true
  '';

  moldConfig = pkgs.lib.optionalString (system == "x86_64-linux") ''
    mkdir -p $PWD/bin
    ln -s ${pkgs.mold}/bin/mold $PWD/bin/lld
    ln -s ${pkgs.mold}/bin/mold $PWD/bin/ld
  '';
in rec {
  packages.patmos-llvm = pkgs.clangStdenv.mkDerivation rec {
    name = "patmos-llvm-${system}";
    src = filteredRepoSrc;

    buildInputs = commonBuildInputs;
    nativeBuildInputs = commonBuildInputs;

    CCACHE_DIR = "${pkgs.ccache}/bin/ccache";
    CCACHE_MAXSIZE = "5G";
    CCACHE_COMPRESS = "1";
    CCACHE_SLOPPINESS = "file_macro,time_macros";

    preConfigurePhase = ''
      ${ccacheConfig}
      ${moldConfig}
    '';

    cmakeFlags =
      [
        "-DCMAKE_BUILD_TYPE=Release"
        "-DLLVM_TARGETS_TO_BUILD=Patmos"
        "-DLLVM_DEFAULT_TARGET_TRIPLE=patmos-unknown-unknown-elf"
        "-DLLVM_ENABLE_PROJECTS=clang;lld"
        "-DCLANG_ENABLE_OBJC_REWRITER=OFF"
        "-DCLANG_ENABLE_STATIC_ANALYZER=OFF"
        "-DCLANG_BUILD_EXAMPLES=OFF"
        "-DLLVM_ENABLE_BINDINGS=OFF"
        "-DLLVM_INSTALL_BINUTILS_SYMLINKS=OFF"
        "-DLLVM_INSTALL_CCTOOLS_SYMLINKS=OFF"
        "-DLLVM_INCLUDE_EXAMPLES=OFF"
        "-DLLVM_INCLUDE_BENCHMARKS=OFF"
        "-DLLVM_INCLUDE_TESTS=OFF"
        "-DLLVM_APPEND_VC_REV=OFF"
        "-DLLVM_ENABLE_WARNINGS=OFF"
        "-DLLVM_ENABLE_PEDANTIC=OFF"
        "-DLLVM_ENABLE_LIBPFM=OFF"
        "-DLLVM_BUILD_INSTRUMENTED_COVERAGE=OFF"
        "-DLLVM_INSTALL_UTILS=ON"
        "-DCLANG_INCLUDE_TESTS=OFF"
        # Ensure MachineInstr::dump() is available even in Release builds
        "-DLLVM_ENABLE_DUMP=ON"
        # dsymutil needs CoreFoundation SDK headers; skip it for this cross-compiler build
        "-DLLVM_TOOL_DSYMUTIL_BUILD=OFF"
      ]
      ++ pkgs.lib.optional (system == "x86_64-linux") [
        "-DCMAKE_CXX_LINKER=mold"
        "-DCMAKE_C_LINKER=mold"
        "-DLLVM_USE_LINKER=mold"
      ];

    configurePhase = ''
      export CCACHE_CPP2=yes
      export CCACHE_SLOPPINESS=${CCACHE_SLOPPINESS}
      cmake -S llvm -B build ${pkgs.lib.escapeShellArgs cmakeFlags}
    '';

    buildPhase = ''
      export CCACHE_MAXSIZE=${CCACHE_MAXSIZE}
      export CCACHE_COMPRESS=${CCACHE_COMPRESS}
      # Full send: every host core. NIX_BUILD_CORES often defaults to 1,
      # so trusting it would serialize the build.
      CORES=$(nproc)
      cmake --build build --parallel "$CORES"
      # Generate TableGen files
      cmake --build build --target llvm-tblgen --parallel "$CORES"
      build/bin/llvm-tblgen -gen-instr-info -I llvm/lib/Target/Patmos -I llvm/include -I llvm/lib/Target llvm/lib/Target/Patmos/Patmos.td -o build/lib/Target/Patmos/PatmosGenInstrInfo.inc
      build/bin/llvm-tblgen -gen-register-info -I llvm/lib/Target/Patmos -I llvm/include -I llvm/lib/Target llvm/lib/Target/Patmos/Patmos.td -o build/lib/Target/Patmos/PatmosGenRegisterInfo.inc
      cp build/lib/Target/Patmos/PatmosGenRegisterInfo* llvm/lib/Target/Patmos/
      build/bin/llvm-tblgen -gen-asm-matcher -I llvm/lib/Target/Patmos -I llvm/include -I llvm/lib/Target llvm/lib/Target/Patmos/Patmos.td -o build/lib/Target/Patmos/PatmosGenAsmMatcher.inc
      cp build/lib/Target/Patmos/PatmosGenAsmMatcher.inc llvm/lib/Target/Patmos/
    '';

    installPhase = ''
      mkdir -p $out
      cmake --install build --prefix "$out"
      ccache --show-stats || true
    '';
  };

  # Faithful to the README-Maintainers test sequence: each suite gets a
  # dedicated derivation so individual legs can be enabled or disabled before
  # the fork gets packaged. Every derivation builds the full LLVM/Clang/LLD
  # tree with test suites enabled and runs the README invocation for its
  # leg. A failing test fails the build; the full lit output is kept in
  # $out/<name>.log.
  mkPatmosTestSuite = testSuite: testTargets: checkScript:
    let drvName = "patmos-${testSuite}-${system}";
    in
    pkgs.clangStdenv.mkDerivation rec {
      name = drvName;
      src = filteredRepoSrc;

      buildInputs = commonBuildInputs ++ [simulator];
      nativeBuildInputs = commonBuildInputs;

      # Without this, stdenv never runs checkPhase and the tests silently pass.
      doCheck = true;

      cmakeFlags =
        [
          "-DCMAKE_BUILD_TYPE=Release"
          "-DLLVM_TARGETS_TO_BUILD=Patmos"
          "-DLLVM_DEFAULT_TARGET_TRIPLE=patmos-unknown-unknown-elf"
          "-DLLVM_ENABLE_PROJECTS=clang;lld"
          "-DCLANG_ENABLE_OBJC_REWRITER=OFF"
          "-DCLANG_ENABLE_STATIC_ANALYZER=OFF"
          "-DCLANG_BUILD_EXAMPLES=OFF"
          "-DLLVM_ENABLE_BINDINGS=OFF"
          "-DLLVM_INSTALL_BINUTILS_SYMLINKS=OFF"
          "-DLLVM_INSTALL_CCTOOLS_SYMLINKS=OFF"
          "-DLLVM_INCLUDE_EXAMPLES=OFF"
          "-DLLVM_INCLUDE_BENCHMARKS=OFF"
          "-DLLVM_APPEND_VC_REV=OFF"
          "-DLLVM_ENABLE_WARNINGS=OFF"
          "-DLLVM_ENABLE_PEDANTIC=OFF"
          "-DLLVM_ENABLE_LIBPFM=OFF"
          "-DLLVM_BUILD_INSTRUMENTED_COVERAGE=OFF"
          "-DLLVM_INSTALL_UTILS=ON"
          # The test suites these derivations exist to run
          "-DLLVM_INCLUDE_TESTS=ON"
          "-DCLANG_INCLUDE_TESTS=ON"
          # Ensure MachineInstr::dump() is available even in Release builds
          "-DLLVM_ENABLE_DUMP=ON"
          # dsymutil must stay enabled: the yaml2obj lit suites added by
          # LLVM_INCLUDE_TESTS=ON declare a dependency on the dsymutil target.
        ]
        ++ pkgs.lib.optional (system == "x86_64-linux") [
          "-DCMAKE_CXX_LINKER=mold"
          "-DCMAKE_C_LINKER=mold"
          "-DLLVM_USE_LINKER=mold"
        ];

      configurePhase = ''
        cmake -S llvm -B build ${pkgs.lib.escapeShellArgs cmakeFlags}
      '';

      buildPhase = ''
        # Full send: every host core. NIX_BUILD_CORES often defaults to 1,
        # so trusting it would serialize the build.
        CORES=$(nproc)
        cmake --build build --parallel "$CORES"
        # Test targets from the README-Maintainers sequence
        cmake --build build --target ${testTargets} --parallel "$CORES"
      '';

      # A failing test fails the derivation; results are never swallowed.
      checkPhase = ''
        export PASIM="${simulator}/bin/pasim"
        export PATH="${simulator}/bin:$PATH"
        set -o pipefail
        cd build
        ${checkScript}
        if ! grep -q "^PASS:" "../${testSuite}.log"; then
          echo "ERROR: no tests reported PASS in ${testSuite}.log" >&2
          exit 2
        fi
        cd ..
      '';

      installPhase = ''
        mkdir -p $out
        cp "${testSuite}.log" "$out/"
      '';
    };

  # README: "make -j4 llc UnitTests && ./bin/llvm-lit ../llvm/test -v --filter=Patmos"
  packages.patmos-llvm-tests = mkPatmosTestSuite "llvm-tests" "UnitTests" ''
    echo "Running Patmos LLVM tests..."
    ./bin/llvm-lit ../llvm/test -v --filter=Patmos 2>&1 | tee ../llvm-tests.log
  '';

  # README: "make -j4 ClangPatmosTestDeps && ./bin/llvm-lit ../clang/test -v --filter=Patmos"
  packages.patmos-clang-tests = mkPatmosTestSuite "clang-tests" "ClangPatmosTestDeps" ''
    echo "Running Patmos Clang tests..."
    ./bin/llvm-lit ../clang/test -v --filter=Patmos 2>&1 | tee ../clang-tests.log
  '';

  # README: "make -j4 lld && ./bin/llvm-lit ../lld/test -v --filter=Patmos"
  packages.patmos-lld-tests = mkPatmosTestSuite "lld-tests" "lld" ''
    echo "Running Patmos LLD tests..."
    ./bin/llvm-lit ../lld/test -v --filter=Patmos 2>&1 | tee ../lld-tests.log
  '';

  checks.patmos-llvm-tests = packages.patmos-llvm-tests;
  checks.patmos-clang-tests = packages.patmos-clang-tests;
  checks.patmos-lld-tests = packages.patmos-lld-tests;

  # Single-file export consumed by pkgs/toolchain to compile libsyms.o
  # when assembling the packaged runtime lib directory.
  sources.patmos-libsyms-ll = "${filteredRepoSrc}/llvm/cmake/platforms/patmos-libsyms.ll";

  # LLVM's bundled lit, consumed by pkgs/cet: the CET suite's config
  # imports lit.llvm modules that only LLVM's own lit provides.
  sources.llvm-lit = "${filteredRepoSrc}/llvm/utils/lit/lit.py";
}
