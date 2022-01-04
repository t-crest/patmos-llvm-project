LLVM Linker (lld) - PATMOS Infos
=================

### INFO

This fork is in its current State NOT WORKING AS INTENDED.

Points i need to fix:

1. I need to test my implementation in lld/ELF/Arch/Patmos.cpp
2. The order of Sections in Outputfiles for Patmos differ from Object-Files created with the currently used Gold-Linker.
[tested with command readelf -e <FILE>]
3. The start of the .text section is in a different place.
(This issue is related to point 2)
4. To test my implementation i need to add Patmos specific test in lld/test/ELF/Patmos.
(Currently the test are just duplicates of SUPPORTED tests.)
5. The Hello_Patmos Programm gives no output linked with lld-link and called with pasim.

### Build

To build the lld linker clang and llvm needed to be built in the same directory

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

...

To test LLD
```
make -j lld && ./bin/llvm-lit ../lld/test -v --filter=Patmos
```
