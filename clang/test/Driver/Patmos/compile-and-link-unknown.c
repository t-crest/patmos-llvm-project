// RUN: %clang --target=patmos %S/helpers/helper-function.c -c -o %t-object.unknown-ext
// RUN: %clang --target=patmos %s %t-object.unknown-ext -o %t
// RUN: llvm-objdump -rd %t | FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests can add object files with unkown extensions to compile
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK-DAG: <helper_source_function>
extern int helper_source_function(int x);

// CHECK-DAG: <main>
int main() {
	return helper_source_function(0);
}


