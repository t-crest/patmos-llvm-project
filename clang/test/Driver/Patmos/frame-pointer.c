// RUN: %clang --target=patmos -### -c -O0 %s 2>&1 | FileCheck %s --check-prefix=FP-ALL
// RUN: %clang --target=patmos -### -c -O1 %s 2>&1 | FileCheck %s --check-prefix=FP-NONE
// RUN: %clang --target=patmos -### -c -O2 %s 2>&1 | FileCheck %s --check-prefix=FP-NONE
// RUN: %clang --target=patmos -### -c -O3 %s 2>&1 | FileCheck %s --check-prefix=FP-NONE
// RUN: %clang --target=patmos -### -c -Os %s 2>&1 | FileCheck %s --check-prefix=FP-NONE
// RUN: %clang --target=patmos -### -c -Oz %s 2>&1 | FileCheck %s --check-prefix=FP-NONE
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// this fucking was so annoying shit to see with me eyes
//
// Tests that Patmos omits the frame pointer by default whenever optimizations
// are enabled, and keeps it at -O0 (where it aids debugging).
//
// Patmos is a bare-metal, WCET-oriented target.
// Reserving $r30 (RFP) as a frame pointer forces an
// additional callee-saved spill/reload pair and an extra
// `mov $r30 = $r31` in the prologue of every non-trivial function
// which inflates both stack-cache traffic and method-cache footprint.
// Since the Patmos ABI does not require a frame pointer for unwinding,
// it must not be on by default in optimized builds.
//
// REGRESSION NOTE: `useFramePointerForTargetByDefault` was moved
// from `clang/lib/Driver/ToolChains/Clang.cpp`
// to `clang/lib/Driver/ToolChains/CommonArgs.cpp`
// by upstream commit
// ea4eb691f495 ("[Flang][Clang] Add support for frame pointers in Flang").
// The downstream `case llvm::Triple::patmos:` was not carried across,
// so Patmos fell through to the function's final `return true` and silently
// gained `-mframe-pointer=all` at every optimization level.
// This test pins the behaviour so the carve-out cannot be dropped again.
// 　　  ∧＿∧　　　／￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣￣
//　　（　´∀｀）　＜　Look who's talking, doing word salad during thesis deadline
//　　（　　　　） 　＼＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿＿
//　　 ｜ ｜　|
//　　（_＿）＿)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// RUN: %clang --target=patmos -### -c -O2 -fno-omit-frame-pointer %s 2>&1 | FileCheck %s --check-prefix=FP-ALL
// RUN: %clang --target=patmos -### -c -O0 -fomit-frame-pointer %s 2>&1 | FileCheck %s --check-prefix=FP-NONE
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that the explicit flags still override the target default in both directions.
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// RUN: %clang --target=patmos -### -c -O2 -pg %s 2>&1 | FileCheck %s --check-prefix=FP-ALL
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that profiling still forces a frame pointer,
// since it needs a walkable call chain.
//
///////////////////////////////////////////////////////////////////////////////////////////////////
// END.

// FP-NONE: "-mframe-pointer=none"
// FP-NONE-NOT: "-mframe-pointer=all"

// FP-ALL: "-mframe-pointer=all"
// FP-ALL-NOT: "-mframe-pointer=none"

int main(int argc, char *argv[]) {
	return argc - 1;
}
