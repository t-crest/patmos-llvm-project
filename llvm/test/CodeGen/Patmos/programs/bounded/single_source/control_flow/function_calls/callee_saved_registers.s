	.text
	.file	"callee_saved_registers.ll"
	.globl	use_all_registers               # -- Begin function use_all_registers
	.p2align	4
	.type	use_all_registers,@function
	.fstart	use_all_registers, .PPGtmp0-use_all_registers, 16
use_all_registers:                      # @use_all_registers
# %bb.0:                                # %entry
.LBB0_0:
  	sres    	      	16
  	sws     	      	[14] = $r28     # 4-byte Folded Spill
  	sws     	      	[13] = $r27     # 4-byte Folded Spill
  	sws     	      	[12] = $r26     # 4-byte Folded Spill
  	sws     	      	[11] = $r25     # 4-byte Folded Spill
  	sws     	      	[10] = $r24     # 4-byte Folded Spill
  	sws     	      	[9] = $r23      # 4-byte Folded Spill
  	sws     	      	[8] = $r22      # 4-byte Folded Spill
  	sws     	      	[7] = $r21      # 4-byte Folded Spill
  	li      	      	$r1 = _1
  	lwc     	      	$r2 = [$r1]
  	nop     	      	
  	sws     	      	[1] = $r2       # 4-byte Folded Spill
  	lwc     	      	$r3 = [$r1]
  	lwc     	      	$r4 = [$r1]
  	lwc     	      	$r5 = [$r1]
  	lwc     	      	$r6 = [$r1]
  	lwc     	      	$r7 = [$r1]
  	lwc     	      	$r8 = [$r1]
  	lwc     	      	$r9 = [$r1]
  	lwc     	      	$r10 = [$r1]
  	lwc     	      	$r11 = [$r1]
  	lwc     	      	$r12 = [$r1]
  	lwc     	      	$r13 = [$r1]
  	lwc     	      	$r14 = [$r1]
  	lwc     	      	$r15 = [$r1]
  	lwc     	      	$r16 = [$r1]
  	lwc     	      	$r17 = [$r1]
  	lwc     	      	$r18 = [$r1]
  	lwc     	      	$r19 = [$r1]
  	lwc     	      	$r20 = [$r1]
  	lwc     	      	$r21 = [$r1]
  	lwc     	      	$r22 = [$r1]
  	lwc     	      	$r23 = [$r1]
  	lwc     	      	$r24 = [$r1]
  	lwc     	      	$r25 = [$r1]
  	lwc     	      	$r26 = [$r1]
  	lwc     	      	$r27 = [$r1]
  	lwc     	      	$r28 = [$r1]
  	lwc     	      	$r2 = [$r1]
  	nop     	      	
  	sws     	      	[0] = $r2       # 4-byte Folded Spill
  	lwc     	      	$r2 = [$r1]
  	nop     	      	
  	sws     	      	[2] = $r2       # 4-byte Folded Spill
  	lwc     	      	$r2 = [$r1]
  	nop     	      	
  	sws     	      	[3] = $r2       # 4-byte Folded Spill
  	lwc     	      	$r2 = [$r1]
  	nop     	      	
  	sws     	      	[4] = $r2       # 4-byte Folded Spill
  	lwc     	      	$r2 = [$r1]
  	nop     	      	
  	sws     	      	[5] = $r2       # 4-byte Folded Spill
  	lwc     	      	$r2 = [$r1]
  	nop     	      	
  	sws     	      	[6] = $r2       # 4-byte Folded Spill
  	lws     	      	$r2 = [1]
  	nop     	      	
  	swc     	      	[$r1] = $r2
  	swc     	      	[$r1] = $r3
  	swc     	      	[$r1] = $r4
  	swc     	      	[$r1] = $r5
  	swc     	      	[$r1] = $r6
  	swc     	      	[$r1] = $r7
  	swc     	      	[$r1] = $r8
  	swc     	      	[$r1] = $r9
  	swc     	      	[$r1] = $r10
  	swc     	      	[$r1] = $r11
  	swc     	      	[$r1] = $r12
  	swc     	      	[$r1] = $r13
  	swc     	      	[$r1] = $r14
  	swc     	      	[$r1] = $r15
  	swc     	      	[$r1] = $r16
  	swc     	      	[$r1] = $r17
  	swc     	      	[$r1] = $r18
  	swc     	      	[$r1] = $r19
  	swc     	      	[$r1] = $r20
  	swc     	      	[$r1] = $r21
  	swc     	      	[$r1] = $r22
  	swc     	      	[$r1] = $r23
  	swc     	      	[$r1] = $r24
  	swc     	      	[$r1] = $r25
  	swc     	      	[$r1] = $r26
  	swc     	      	[$r1] = $r27
  	swc     	      	[$r1] = $r28
  	lws     	      	$r2 = [0]
  	nop     	      	
  	swc     	      	[$r1] = $r2
  	lws     	      	$r2 = [2]
  	nop     	      	
  	swc     	      	[$r1] = $r2
  	lws     	      	$r2 = [3]
  	nop     	      	
  	swc     	      	[$r1] = $r2
  	lws     	      	$r2 = [4]
  	nop     	      	
  	swc     	      	[$r1] = $r2
  	lws     	      	$r2 = [5]
  	nop     	      	
  	swc     	      	[$r1] = $r2
  	lws     	      	$r2 = [6]
  	nop     	      	
  	swc     	      	[$r1] = $r2
  	lws     	      	$r21 = [7]
  	lws     	      	$r22 = [8]
  	lws     	      	$r23 = [9]
  	lws     	      	$r24 = [10]
  	lws     	      	$r25 = [11]
  	lws     	      	$r26 = [12]
  	lws     	      	$r27 = [13]
  	ret     	      	
  	lws     	      	$r28 = [14]
  	nop     	      	
  	sfree   	      	16
.PPGtmp0:
.PPGfunc_end0:
	.size	use_all_registers, .PPGfunc_end0-use_all_registers
                                        # -- End function
	.globl	main                            # -- Begin function main
	.p2align	4
	.type	main,@function
	.fstart	main, .PPGtmp1-main, 16
main:                                   # @main
# %bb.0:                                # %entry
.LBB1_0:
	#APP

  	sres    	      	8

  	mov     	      	$r21 = $r3

  	mov     	      	$r22 = $r3

  	mov     	      	$r23 = $r3

  	mov     	      	$r24 = $r3

  	mov     	      	$r25 = $r3

  	mov     	      	$r26 = $r3

  	mov     	      	$r27 = $r3

  	mov     	      	$r28 = $r3

  	mfs     	      	$r9 = $s7

  	sws     	      	[0] = $r9

  	mfs     	      	$r9 = $s8

  	sws     	      	[1] = $r9


	#NO_APP
  	li      	      	$r1 = use_all_registers
	#APP

  	callnd  	      	$r1

  	lws     	      	$r9 = [1]

  	lws     	      	$r10 = [0]

  	mts     	      	$s8 = $r9

  	mts     	      	$s7 = $r10

  	li      	      	$r1 = 0

  	add     	      	$r1 = $r1, $r21

  	add     	      	$r1 = $r1, $r22

  	add     	      	$r1 = $r1, $r23

  	add     	      	$r1 = $r1, $r24

  	add     	      	$r1 = $r1, $r25

  	add     	      	$r1 = $r1, $r26

  	add     	      	$r1 = $r1, $r27

  	add     	      	$r1 = $r1, $r28

  	sfree   	      	8


	#NO_APP
  	retnd   	      	
.PPGtmp1:
.PPGfunc_end1:
	.size	main, .PPGfunc_end1-main
                                        # -- End function
	.type	_1,@object                      # @_1
	.data
	.globl	_1
	.p2align	2
_1:
	.word	1                               # 0x1
	.size	_1, 4

	.section	".note.GNU-stack","",@progbits
