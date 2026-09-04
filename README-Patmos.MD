Patmos LLVM
================

This is a fork of the LLVM Project that adds a target for the Patmos Architecture.

# Installation (Fast way)

Install nix or lix, and then run the following command in your terminal:

```zsh
sudo nix develop github:t-crest/patmos-llvm-project/main --accept-flake-config
```

Nope, you cannot _not_ `sudo`, you cannot _not_ `--accept-flake-config`, otherwise you will suffer the pain of being installing the compiler for 2 hours.

# Installation

Prebuilt releases are [published on Github](https://github.com/t-crest/patmos-llvm-project/releases) as `.tar.gz` files.
To install, download the tarball that fits your platform into you desired installation folder (usually `t-crest/local`.
Then open a terminal in that folder and run the following command (where `<tarfile>` is the release tar file you downloaded):

```
tar -xvf <tarfile>
```

This will correctly install all artifacts into the right paths. You are now ready to use the compiler.
Remember to set your `PATH` to point to `t-crest/local/bin`.

## Nix Usage (Prefixed Tools)

The flake exports Patmos-specific wrappers so your host LLVM/Clang is not overridden.
Use `patmos-*` commands (`patmos-clang`, `patmos-llc`, `patmos-llvm-objdump`, ...).

### Channels

- `patmos-prefixed` (default): release channel wrapper.

### Quick shell usage

```zsh
# Default channel (currently release wrapper; falls back to source if artifact metadata is not set)
nix shell -p github:t-crest/patmos-llvm-project/main#patmos-prefixed

# If you are already from the repo
nix develop

# Check that it works
patmos-clang --version
```

### Dev shells

```zsh
# Default dev shell (release wrapper)
nix develop github:t-crest/patmos-llvm-project/main

# Example compile command
patmos-clang -O2 -c fibonacci-test.cpp -o fibonacci-test.o
```

### Persistent install (AT YER OWN RISK)

```zsh
nix profile install github:t-crest/patmos-llvm-project/main#patmos-prefixed
patmos-clang --version
```

# Development

### Requirements

Platform:

- Ubuntu 18/20+
- MacOs 10.15+
- Windows Subsystem for Linux running Ubuntu 18/20+
- NixOS 25.11+

Tools:

- `gcc` v12+ or `clang` v17+
- `git` v2.17.1
- `cmake` v3.30.0+
- `make` v4.1
- GNU tar v1.29 (must be available on the path as `tar`)
- Binutils v2.30+ as default (E.g., `ar` must point to binutils `ar`)
- Patmos Simulator Tools v1.0.2

### Branches

- `main`: Primary development branch that mirror`s the LLVM's release branch,
- `main-rust`: Secondary development branch that mirror's the Rust's LLVM release branch.

### Cloning

The LLVM project is very large so we can save some time by avoiding cloning the part we don't use for Patmos.
The following commands with clone only the parts of the Patmos LLVM repository that we actually use:

```
git clone --filter=blob:none --sparse git@github.com:t-crest/patmos-llvm-project
cd patmos-llvm-project
git sparse-checkout add /.github /llvm /clang /compiler-rt /lld /libunwind /libc
```

### Build

Ensure all the above requirements are met and that the tools are available on the PATH.

Then you must create a build folder.
This folder doesn't have to be in the LLVM Project root folder, but that is where we will put it in this example.
In this folder, you must setup the build with `cmake` (referencing the LLVM subfolder), after which you can build:

```zsh
mkdir -p build
cd build
cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DLLVM_TARGETS_TO_BUILD="Patmos" -DLLVM_DEFAULT_TARGET_TRIPLE=patmos-unknown-unknown-elf -DLLVM_ENABLE_PROJECTS="clang;lld" -DCLANG_ENABLE_ARCMT=false -DCLANG_ENABLE_STATIC_ANALYZER=false -DCLANG_BUILD_EXAMPLES=false -DLLVM_ENABLE_BINDINGS=false -DLLVM_INSTALL_BINUTILS_SYMLINKS=false -DLLVM_INSTALL_CCTOOLS_SYMLINKS=false -DLLVM_INCLUDE_EXAMPLES=false -DLLVM_INCLUDE_BENCHMARKS=false -DLLVM_APPEND_VC_REV=false -DLLVM_ENABLE_WARNINGS=false -DLLVM_ENABLE_PEDANTIC=false -DLLVM_ENABLE_LIBPFM=false -DLLVM_BUILD_INSTRUMENTED_COVERAGE=false -DLLVM_INSTALL_UTILS=false
make -j
```

If you need to work on Compiler-RT, you can do the following (from the repo root):

```zsh
mkdir -p build-compiler-rt
cmake -S compiler-rt -B build-compiler-rt \
  -DCMAKE_TOOLCHAIN_FILE=compiler-rt/cmake/patmos-clang-toolchain.cmake \
  -DCMAKE_PROGRAM_PATH="../build/bin" \
  -DLLVM_CMAKE_DIR="../build/lib/cmake/llvm" \
  -DCOMPILER_RT_INCLUDE_TESTS=ON
cmake --build build-compiler-rt -j
```

You must build Compiler-RT, or the two `.o` files will be missing when you use the tool.

If you need to build the standard library (newlib):

```zsh
git clone https://github.com/t-crest/patmos-newlib
mkdir build-newlib
cd build-newlib
../patmos-newlib/configure --target=patmos-unknown-unknown-elf AR_FOR_TARGET="$(pwd)/../build/bin/llvm-ar" CC_FOR_TARGET="$(pwd)/../build/bin/clang" CFLAGS_FOR_TARGET="-target patmos-unknown-unknown-elf -O2 -emit-llvm -Wno-error -Wno-error=deprecated-non-prototype -Wno-error=invalid-noreturn -D__GLIBC_USE\(...\)=0 -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types" RANLIB_FOR_TARGET="$(pwd)/../build/bin/llvm-ranlib" LD_FOR_TARGET="$(pwd)/../build/bin/clang"
make
```

This compiles std into bitcode files, such that they are ready to publish with the compiler.

### Compiling and inspecting output

```zsh
# Compile the code
./build/bin/clang++ --target=patmos-unknown-unknown-elf -O2 -c ./fibonacci-test.cpp -o ./fibonacci-test.o

# Dump the object code
./build/bin/llvm-objdump -d ./fibonacci-test.o

# Generate the .pml file for Platin
# Important: use -mserialize-pml, not -mserialize
./build/bin/clang++ --target=patmos-unknown-unknown-elf -O2 -mserialize-pml=fibonacci.pml -c ./fibonacci-test.cpp -o fibonacci.out

# Run Platin
./.platin-inst/bin/platin wcet -i fibonacci.pml -b fibonacci.out -e main --report --disable-ait --objdump-command ./build/bin/llvm-objdump
```