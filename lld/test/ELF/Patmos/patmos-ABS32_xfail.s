// REQUIRES: patmos

// XFAIL: *

// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %s -o %t.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs255.s -o %t255.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs256.s -o %t256.o
// RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf %S/../Inputs/abs257.s -o %t257.o

.globl _start
_start:
.data
  .word foo + 0xfffffeff
  .word foo - 0x80000100

// RUN: ld.lld %t.o %t255.o -o /dev/null 2>&1
// RUN: ld.lld %t.o %t257.o -o /dev/null 2>&1