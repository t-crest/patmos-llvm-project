{
  pkgs,
  system,
}: let
  # Use a recursive attribute set to allow circular references
  self = rec {
    llvm = import ./llvm {
      inherit pkgs system;
      simulator = self.simulator.packages.patmos-simulator;
    };
    simulator = import ./simulator {inherit pkgs system;};
    newlib = import ./newlib {
      inherit pkgs system;
      patmos-llvm = llvm.packages.patmos-llvm;
      patmos-simulator = simulator.packages.patmos-simulator;
    };
    compiler-rt = import ./compiler-rt {
      inherit pkgs system;
      patmos-llvm = llvm.packages.patmos-llvm;
      patmos-newlib = newlib.packages.patmos-newlib;
      patmos-simulator = simulator.packages.patmos-simulator;
    };
    toolchain = import ./toolchain {
      inherit pkgs system;
      patmos-llvm = llvm.packages.patmos-llvm;
      patmos-newlib = newlib.packages.patmos-newlib;
      patmos-compiler-rt = compiler-rt.packages.patmos-compiler-rt;
      patmos-simulator = simulator.packages.patmos-simulator;
      patmos-libsyms-ll = llvm.sources.patmos-libsyms-ll;
    };
    binary = import ./binary {
      inherit pkgs system;
      patmos-toolchain-src = toolchain.packages.patmos-toolchain;
    };
    prefixed = import ./prefixed {
      inherit pkgs system;
      patmos-toolchain-src = toolchain.packages.patmos-toolchain;
      patmos-toolchain-release = binary.packages.patmos-bin-release;
      patmos-simulator = simulator.packages.patmos-simulator;
    };
    tarball = import ./tarball {
      inherit pkgs system;
      patmos-toolchain = toolchain.packages.patmos-toolchain;
      patmos-simulator = simulator.packages.patmos-simulator;
      # The tarball is gated on every test suite passing.
      patmos-llvm-tests = llvm.checks.patmos-llvm-tests;
      patmos-clang-tests = llvm.checks.patmos-clang-tests;
      patmos-lld-tests = llvm.checks.patmos-lld-tests;
      patmos-compiler-rt-tests = compiler-rt.checks.patmos-compiler-rt-tests;
      patmos-package-tests = prefixed.checks.patmos-package-tests;
    };
  };
in {
  packages = {
    patmos-llvm = self.llvm.packages.patmos-llvm;
    patmos-src = self.llvm.packages.patmos-llvm;
    patmos-llvm-tests = self.llvm.packages.patmos-llvm-tests;
    patmos-clang-tests = self.llvm.packages.patmos-clang-tests;
    patmos-lld-tests = self.llvm.packages.patmos-lld-tests;
    patmos-simulator = self.simulator.packages.patmos-simulator;
    patmos-newlib = self.newlib.packages.patmos-newlib;
    patmos-compiler-rt = self.compiler-rt.packages.patmos-compiler-rt;
    patmos-toolchain = self.toolchain.packages.patmos-toolchain;
    patmos-bin-release = self.binary.packages.patmos-bin-release;
    patmos-prefixed = self.prefixed.packages.patmos-prefixed;
    patmos-tarball = self.tarball.packages.patmos-tarball;
    patmos-prefixed-src = self.prefixed.packages.patmos-prefixed-src;
    patmos-prefixed-release = self.prefixed.packages.patmos-prefixed-release;
    patmos = self.prefixed.packages.patmos-prefixed;
    default = self.prefixed.packages.patmos-prefixed;
  };
  # The full LLVM/Clang/LLD test trees do not fit the macOS CI runners, so
  # the per-suite derivations are checks on Linux only; compiler-rt and the
  # packaged-toolchain smoke test run everywhere.
  checks =
    {
      patmos-compiler-rt-tests = self.compiler-rt.checks.patmos-compiler-rt-tests;
      patmos-package-tests = self.prefixed.checks.patmos-package-tests;
    }
    // pkgs.lib.optionalAttrs pkgs.stdenv.isLinux {
      patmos-llvm-tests = self.llvm.checks.patmos-llvm-tests;
      patmos-clang-tests = self.llvm.checks.patmos-clang-tests;
      patmos-lld-tests = self.llvm.checks.patmos-lld-tests;
    };
  devShells = rec {
    default = (pkgs.mkShell.override {stdenv = pkgs.clangStdenv;}) {
      buildInputs = [
        self.prefixed.packages.patmos-prefixed
      ];
      PATMOS_TRIPLE = "patmos-unknown-unknown-elf";
      PATMOS_SYSROOT = "${self.binary.packages.patmos-bin-release}/newlib-sysroot";
      PASIM = "${self.simulator.packages.patmos-simulator}/bin/pasim";
      shellHook = ''
        echo "Patmos dev environment (prefixed tools)"
        export CC="patmos-clang"
        export CXX="patmos-clang++"
      '';
    };
    release = (pkgs.mkShell.override {stdenv = pkgs.clangStdenv;}) {
      buildInputs = [
        self.prefixed.packages.patmos-prefixed-release
      ];
      PATMOS_TRIPLE = "patmos-unknown-unknown-elf";
      PATMOS_SYSROOT = "${self.binary.packages.patmos-bin-release}/newlib-sysroot";
      PASIM = "${self.simulator.packages.patmos-simulator}/bin/pasim";
      shellHook = ''
        echo "Patmos release-channel environment (prefixed tools)"
        export CC="patmos-clang"
        export CXX="patmos-clang++"
      '';
    };
    src = (pkgs.mkShell.override {stdenv = pkgs.clangStdenv;}) {
      buildInputs = [
        self.prefixed.packages.patmos-prefixed-src
      ];
      PATMOS_TRIPLE = "patmos-unknown-unknown-elf";
      PATMOS_SYSROOT = "${self.toolchain.packages.patmos-toolchain}/newlib-sysroot";
      PASIM = "${self.simulator.packages.patmos-simulator}/bin/pasim";
      shellHook = ''
        echo "Patmos source-build environment (prefixed tools)"
        export CC="patmos-clang"
        export CXX="patmos-clang++"
      '';
    };

    # Friendly aliases: align `nix develop` attrs with exported package names.
    "patmos-prefixed" = default;
    "patmos-prefixed-release" = release;
    "patmos-prefixed-src" = src;
  };
}
