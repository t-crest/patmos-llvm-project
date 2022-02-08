# REQUIRES: patmos

# XFAIL: *

# RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

# RUN: ld.lld %t.patmos.o --defsym foo=-1 -o %t.patmos
# RUN: ld.lld %t.patmos.o --defsym foo=4294967296 -o %t.patmos

.global _start
_start:
    add     $r1 = $r1, foo
    sub     $r1 = $r1, bar
    xor     $r1 = $r1, foo
    or      $r1 = $r1, foo
    and     $r1 = $r1, foo
    nor     $r1 = $r1, foo
    shadd   $r1 = $r1, foo
    shadd2  $r1 = $r1, foo

.global bar
bar = 4294967296

