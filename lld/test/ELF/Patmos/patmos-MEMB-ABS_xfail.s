# XFAIL: *

# RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

# RUN: ld.lld %t.patmos.o --defsym foo=64 --defsym bar=-65 -o %t.patmos


.global _start
_start:
    lbs  $r1 = [$r2 + foo]
    lbl  $r1 = [$r2 + foo]
    lbc  $r1 = [$r2 + bar]
    lbm  $r1 = [$r2 + bar]