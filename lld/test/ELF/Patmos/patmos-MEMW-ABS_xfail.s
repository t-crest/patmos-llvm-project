# REQUIRES: patmos

# XFAIL: *

# RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

# RUN: ld.lld %t.patmos.o --defsym foo=256 --defsym bar=-257 -o %t.patmos


.global _start
_start:
    lws  $r1 = [$r2 + foo]
    lwl  $r1 = [$r2 + foo]
    lwc  $r1 = [$r2 + bar]
    lwm  $r1 = [$r2 + bar]