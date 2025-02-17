; RUN: EXEC_ARGS="0=0 1=1 2=3 3=6 4=10"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
; 
; Tests a loop with only one block
; 
;//////////////////////////////////////////////////////////////////////////////////////////////////

@_1 = global i32 1

define i32 @main(i32 %iteration_count)  {
entry:
  br label %for.cond

for.cond:
  %x.0 = phi i32 [ 0, %entry ], [ %add1, %for.cond ]
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.cond ]
  %0 = load volatile i32, i32* @_1
  %add = add nsw i32 %i.0, %0
  %add1 = add nsw i32 %x.0, %add
  %inc = add nsw i32 %i.0, 1
  %cmp = icmp slt i32 %i.0, %iteration_count
  call void @llvm.loop.bound(i32 0, i32 4)
  br i1 %cmp, label %for.cond, label %for.end

for.end:
  ret i32 %x.0
}

declare void @llvm.loop.bound(i32, i32)
