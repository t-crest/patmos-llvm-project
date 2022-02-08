// REQUIRES: patmos

// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %s -o %t.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs255.s -o %t255.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs256.s -o %t256.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs257.s -o %t257.o

.globl _start
_start:
.data
  .word foo + 0xfffffeff
  .word foo - 0x80000100

// RUN: ld.lld %t.o %t256.o -o %t2
// RUN: llvm-objdump -s --section=.data %t2 | FileCheck %s

// CHECK: Contents of section .data:
// ?????:   S = 0x100, A = 0xfffffeff
//          S + A = 0xffffffff
// ?????+4: S = 0x100, A = -0x80000100
//          S + A = 0x80000000
// CHECK-NEXT: ffffffff 80000000
