// RUN: %clang --target=patmos %S/helpers/helper-function.c -c -o %t-object.o
// RUN: %clang --target=patmos %s %t-object.o -o %t
// RUN: llvm-objdump -rd %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests can add object file to compile command
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK-DAG: <helper_source_function>
extern int helper_source_function(int x);

// CHECK-DAG: <main>
int main() {
	return helper_source_function(0);
}


