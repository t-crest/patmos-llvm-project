; RUN: EXEC_ARGS="1=1 2=2"; \
; RUN: WITH_DEBUG=true; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Test stack allocation within a loop
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

; CHECK-LABEL: main:
define i32 @main(i32 %value)  {
entry:
  %i = alloca i32, align 4
  store i32 0, i32* %i, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %0 = load i32, i32* %i, align 4
  %cmp = icmp slt i32 %0, 5
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %x = alloca i32, align 4
  %1 = load i32, i32* %i, align 4
  store i32 %1, i32* %x, align 4
  %2 = load i32, i32* %i, align 4
  %add = add nsw i32 %2, 1
  store i32 %add, i32* %i, align 4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  ret i32 %value
}