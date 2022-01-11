# RUN: llvm-mc -filetype=obj -triple=patmos-unknown-unknown-elf -mattr=-relax %s -o %t.patmos.o

# RUN: ld.lld %t.patmos.o -o %t.patmos
# RUN: llvm-objdump -d %t.patmos | FileCheck %s
# CHECK:      04 c0 00 04     br   4
# CHECK:      04 c0 00 02     br   2

.global _start
_start:
    br    bar
    br    foo
    nop
    

.global foo, bar
foo:  
    nop
bar:
    nop