{
  pkgs,
  system,
  patmos-llvm,
  patmos-newlib,
  patmos-compiler-rt,
  patmos-simulator,
  patmos-libsyms-ll,
}: {
  packages.patmos-toolchain = pkgs.stdenv.mkDerivation {
    name = "patmos-toolchain-${system}";
    dontUnpack = true;
    buildInputs = [patmos-llvm patmos-newlib patmos-compiler-rt patmos-simulator];

    installPhase = ''
      mkdir -p $out
      cp -r ${patmos-llvm}/* $out/
      mkdir -p $out/newlib-sysroot
      cp -r ${patmos-newlib}/* $out/newlib-sysroot/
      mkdir -p $out/compiler-rt-build
      cp -r ${patmos-compiler-rt}/* $out/compiler-rt-build/
      mkdir -p $out/patmos-tools
      cp -r ${patmos-simulator}/* $out/patmos-tools/

      # Store artifacts are read-only; make the copies writable before
      # assembling the runtime lib directory.
      chmod -R u+w $out

      # The Patmos driver locates runtime libs and includes relative to the
      # clang binary at <root>/<triple>/, mirroring the CI PatmosPackage
      # tarball layout (see clang/lib/Driver/ToolChains/Patmos.cpp). Assemble
      # that directory from the newlib install, and add librt.a (compiler-rt
      # builtins, renamed) plus libsyms.o (compiled from patmos-libsyms.ll)
      # as PatmosPackage does.
      NEWLIB_TARGET=$(dirname "$(dirname "$(find $out/newlib-sysroot -name libc.a -print -quit)")")
      if [ -z "$NEWLIB_TARGET" ] || [ ! -d "$NEWLIB_TARGET" ]; then
        echo "ERROR: patmos-unknown-unknown-elf dir not found in newlib-sysroot" >&2
        exit 2
      fi
      mkdir -p $out/patmos-unknown-unknown-elf
      cp -r "$NEWLIB_TARGET"/. $out/patmos-unknown-unknown-elf/
      if [ ! -f $out/patmos-unknown-unknown-elf/lib/libc.a ]; then
        echo "ERROR: libc.a missing after assembling the runtime directory" >&2
        exit 2
      fi
      LIBRT=$(find ${patmos-compiler-rt} -name 'libclang_rt.builtins-patmos.a' -print -quit)
      if [ -z "$LIBRT" ]; then
        echo "ERROR: libclang_rt.builtins-patmos.a not found in compiler-rt output" >&2
        exit 2
      fi
      cp "$LIBRT" $out/patmos-unknown-unknown-elf/lib/librt.a
      ${patmos-llvm}/bin/clang -c -emit-llvm ${patmos-libsyms-ll} -o $out/patmos-unknown-unknown-elf/lib/libsyms.o -Wno-override-module
    '';
  };
}
