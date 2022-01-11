# RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

# RUN: ld.lld %t.patmos.o --defsym foo=16 --defsym bar=32 -o %t.patmos
# RUN: llvm-objdump -d %t.patmos | FileCheck %s
# CHECK:      02 82 20 04     lws  $r1 = [$r2 + 4]
# CHECK:      02 82 20 84     lwl  $r1 = [$r2 + 4]
# CHECK:      02 82 21 08     lwc  $r1 = [$r2 + 8]
# CHECK:      02 82 21 88     lwm  $r1 = [$r2 + 8]




.global _start
_start:
    lws  $r1 = [$r2 + foo]
    lwl  $r1 = [$r2 + foo]
    lwc  $r1 = [$r2 + bar]
    lwm  $r1 = [$r2 + bar]