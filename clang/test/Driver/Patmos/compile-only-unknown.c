// RUN: %clang --target=patmos %S/helpers/helper-main.c -c -o %t-object.unknown-ext
// RUN: %clang --target=patmos %S/helpers/helper-function.c -c -o %t-object2.unknown-ext
// RUN: %clang --target=patmos %t-object.unknown-ext  %t-object2.unknown-ext -o %t
// RUN: llvm-objdump -rd %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests can compile only object files with unknown extension
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK-DAG: <main>
// CHECK-DAG: <helper_source_function>

