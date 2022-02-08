# REQUIRES: patmos

# XFAIL: *

# RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

# RUN: ld.lld %t.patmos.o --defsym foo=128 --defsym bar=-129 -o %t.patmos


.global _start
_start:
    lhs  $r1 = [$r2 + foo]
    lhl  $r1 = [$r2 + foo]
    lhc  $r1 = [$r2 + bar]
    lhm  $r1 = [$r2 + bar]