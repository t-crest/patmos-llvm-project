// RUN: %clang --target=patmos %S/helpers/helper-function.c -c -o %t-object.o
// RUN: %clang --target=patmos %S/helpers/helper-function2.c -c -o %t-object2.o
// RUN: ar crs %t-archive.a %t-object.o %t-object2.o
// RUN: %clang --target=patmos %s %t-archive.a -o %t
// RUN: llvm-objdump -rd %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests can add archive file to compile command
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK-DAG: <helper_source_function>
extern int helper_source_function(int x);
// CHECK-DAG: <helper_source_function2>
extern int helper_source_function2(int x);

// CHECK-DAG: <main>
int main() {
	return helper_source_function2(helper_source_function(0));
}


