# REQUIRES: patmos

# XFAIL: *

# RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

# RUN: ld.lld %t.patmos.o --defsym foo=16777216 --defsym bar=8 -o %t.patmos

.global _start
_start:
    call    foo
    call    bar

.global bar
bar = -5