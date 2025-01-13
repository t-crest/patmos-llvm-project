// RUN: %clang --target=patmos %S/helpers/helper-function.c -c -o %t-object.o
// RUN: %clang --target=patmos %S/helpers/helper-function2.c -c -o %t-object2.o
// RUN: ar cr %t-archive.a %t-object.o %t-object2.o
// RUN: %clang --target=patmos %s -c -o %t-object3.o
// RUN: ar cr %t-archive2.a %t-object3.o
// RUN: %clang -v --target=patmos %t-archive.a %t-archive2.a -o %t
// RUN: llvm-objdump -rd %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests can compile only archive files
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK-DAG: <helper_source_function>
extern int helper_source_function(int x);
// CHECK-DAG: <helper_source_function2>
extern int helper_source_function2(int x);

// CHECK-DAG: <main>
int main(int x) { 
	return helper_source_function(helper_source_function2(x));
}