// RUN: %clang --target=patmos %S/helpers/helper-main.c -c -o %t-object.o
// RUN: %clang --target=patmos %S/helpers/helper-function.c -c -o %t-object2.o
// RUN: %clang --target=patmos %t-object.o  %t-object2.o -o %t
// RUN: llvm-objdump -rd %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests can compile only an object files
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK-DAG: <main>
// CHECK-DAG: <helper_source_function>

