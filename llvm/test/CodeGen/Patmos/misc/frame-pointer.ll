; RUN: llc %s -o - | FileCheck %s
;//////////////////////////////////////////////////////////////////////////////////////////////////
; Ensure the Patmos backend honours the "frame-pointer" function attribute that the driver sets.
;
; This is the backend half of the contract tested by
; `clang/test/Driver/Patmos/frame-pointer.c`. The driver decides *whether* to ask for a frame
; pointer; this test pins what that request costs, so a regression on either side is visible.
;
; On a WCET-oriented target that difference is charged to every call, so it must not be enabled by accident.
; Enabling it causes some performance regression.
; See the said C driver for more details. 'clang/test/Driver/Patmos/frame-pointer.c'
;//////////////////////////////////////////////////////////////////////////////////////////////////
; END

target triple = "patmos-unknown-unknown-elf"

declare void @escape(ptr)

; A frame pointer was explicitly requested
;
; CHECK-LABEL: with_frame_pointer:
; CHECK: sws {{.*}}= $r30
; CHECK: mov $r30 = $r31
; CHECK: lwc $r1 = [$r30]
; CHECK: mov $r31 = $r30
; CHECK: lws $r30 = [{{[0-9]+}}]
define i32 @with_frame_pointer() #0 {
  %slot = alloca [4 x i32]
  call void @escape(ptr %slot)
  %v = load i32, ptr %slot
  ret i32 %v
}

; No frame pointer was requested
;
; CHECK-LABEL: without_frame_pointer:
; CHECK: lwc $r1 = [$r31]
; CHECK-NOT: mov $r30 = $r31
; CHECK-NOT: mov $r31 = $r30
; CHECK: .Lfunc_end1:
define i32 @without_frame_pointer() #1 {
  %slot = alloca [4 x i32]
  call void @escape(ptr %slot)
  %v = load i32, ptr %slot
  ret i32 %v
}

;
; CHECK-LABEL: frame_address_taken:
; CHECK: mov $r30 = $r31
define ptr @frame_address_taken() #1 {
  %fa = call ptr @llvm.frameaddress.p0(i32 0)
  ret ptr %fa
}

declare ptr @llvm.frameaddress.p0(i32 immarg)

attributes #0 = { "frame-pointer"="all" }
attributes #1 = { "frame-pointer"="none" }



;/////////
;//           MY INSANITY AND SHEER DISSOCIATION JUST KICKED
;//                 IN ! FANTASTIC !
;//                   __      _______ ______
;//                ╱    /\__/\       //     ╲╲
;//        ______⊂╱    ( ´∇`  )     // ⊃     ||╲ フ 🡖
;//      ,´__▔▔▔▔╱  ▔╱▔  ⌒▔▔▔▔╱▔▔▔▔ 🡖▔ ▔▔▔▔▔🡖 ▔▔▔▔ |
;//    ,╱_ _╱   /-o—/ ___ ╱▔▔╱ ___/\  |     ▔ | /\__|
;//   ,========————´=============/⌒ ╲=/=======||🡖 ||
;//   | __  |  GAY!  |   __ "    |⌒| |/    ___/|  )╯
;//   )|🞕|_∈≡≡≡≡≡≡≡≡≡∋__|🞕|"  __|| ╯ ╯__ -‒‒‒‒‒┘  ╯  vrromm
;//   ▔╲ ▔╲__╯▔▔▔▔▔▔▔▔三三三▔╲  ╲__╯ ▔▔     三三三三╯ vroooom
;//     三三三三三三三三三三三三三三三三三三三三三三三三三三三三
;//       三三三三三三三三三三三三三三三三三三三三三三三三三三三三