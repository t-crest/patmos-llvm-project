; RUN: EXEC_ARGS="0=0 1=0 8=0 9=1 10=1 17=2 18=4 19=2 27=9 45=25"; \
; RUN: %test_execution
; XFAIL: *
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; 
; Tests having an edge that exits a loop from its inner loop
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

@_1 = global i32 1

define i32 @main(i32 %x) {
entry:
  %0 = and i32 %x, 7
  %1 = and i32 %x, 56
  %2 = lshr i32 %1, 3
  br label %loop1

loop1:
  %loop1.i = phi i32 [0, %entry],[ %loop1.i.inc, %loop2]
  %result = phi i32 [0, %entry],[ %result.loop2, %loop2]
  ;%_1.loop1 = load volatile i32, i32 * @_1
  %loop1.i.inc = add i32 %loop1.i, 1
  %loop1.end = icmp uge i32 %loop1.i, %2
  call void @llvm.loop.bound(i32 0, i32 5)
  br i1 %loop1.end, label %end, label %loop2
  
loop2:
  %loop2.i = phi i32 [0, %loop1],[ %loop2.i.inc, %loop2.post]
  %result.loop2 = phi i32 [%result, %loop1],[ %result.loop2.inc, %loop2.post]
  ;%_1.loop2 = load volatile i32, i32 * @_1
  %loop2.i.inc = add i32 %loop2.i, 1
  %result.loop2.inc = add i32 %result.loop2, 1
  %loop2.loop = icmp ult i32 %loop2.i, %0
  call void @llvm.loop.bound(i32 0, i32 5)
  br i1 %loop2.loop, label %loop2.post, label %loop1
  
loop2.post:
  %loop2.break = icmp uge i32 %loop2.i, %2
  br i1 %loop2.break, label %end, label %loop2

end:
  %result.end = phi i32 [%result, %loop1], [%result.loop2, %loop2.post]
  ret i32 %result.end
}

declare void @llvm.loop.bound(i32, i32)
