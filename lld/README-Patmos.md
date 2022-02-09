LLVM Linker (lld) - PATMOS Infos
=================

### INFO

This fork is in its current State links SIMPLE programms as well as the LLVM testcases and the Testcases in Github Actions.

My Points that are still unclear/ need checking:

1. The order of Sections in Outputfiles for Patmos differs from Object-Files created with the currently used Gold-Linker.
[tested with command readelf -e <FILE>]
2. How to call LLD from Clang

### Build

To build the lld linker, clang and llvm needed to be built in the same directory

```
mkdir -p build
cd build
cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DLLVM_TARGETS_TO_BUILD="Patmos" -DLLVM_DEFAULT_TARGET_TRIPLE=patmos-unknown-unknown-elf -DLLVM_ENABLE_PROJECTS="clang;llvm;lld" -DCLANG_ENABLE_ARCMT=false -DCLANG_ENABLE_STATIC_ANALYZER=false -DCLANG_BUILD_EXAMPLES=false -DLLVM_ENABLE_BINDINGS=false -DLLVM_INSTALL_BINUTILS_SYMLINKS=false -DLLVM_INSTALL_CCTOOLS_SYMLINKS=false -DLLVM_INCLUDE_EXAMPLES=false -DLLVM_INCLUDE_BENCHMARKS=false -DLLVM_APPEND_VC_REV=false -DLLVM_ENABLE_WARNINGS=false -DLLVM_ENABLE_PEDANTIC=false -DLLVM_ENABLE_LIBPFM=false -DLLVM_BUILD_INSTRUMENTED_COVERAGE=false -DLLVM_INSTALL_UTILS=false
make -j
```

### Calling - LLD-Linker (standalone)

Calling the built lld-linker (standalone) 


Following command can be used to call the lld-linker
(from within the build directory):

```
bin/ld.lld --nostdlib --static --defsym=__heap_start=end --defsym=__heap_end=0x100000 --defsym=_shadow_stack_base=0x1f8000 --defsym=_stack_cache_base=0x200000 --verbose -o <OUTPUTFILE> <INPUTFILE>
```

### Test


To test LLD

```
make -j lld && ./bin/llvm-lit ../lld/test -v
```

To test LLD (only patmos testcases)

```
make -j lld && ./bin/llvm-lit ../lld/test -v --filter=Patmos
```

### Changes outside of the lld folder

1. For testing with LLVM testcases llvm/test/CodeGen/Patmos/* changed from patmos-ld to ld.lld
2. Change Github Actions to include LLD
3. Adapt README-Patmos.MD in parentfolder