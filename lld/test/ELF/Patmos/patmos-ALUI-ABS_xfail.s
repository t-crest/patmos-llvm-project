# REQUIRES: patmos

# XFAIL: *

# RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

# RUN: ld.lld %t.patmos.o --defsym foo=4096 -o %t.patmos

.global _start
_start:
    sl      $r1 = $r1, foo
    sr      $r1 = $r1, bar
    sra     $r1 = $r1, bar

.global bar
bar = 4096

