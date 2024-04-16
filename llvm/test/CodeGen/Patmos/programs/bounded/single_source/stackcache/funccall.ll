; RUN: EXEC_ARGS="1=3 2=4"; \
; RUN: WITH_DEBUG=true; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that incrementing twice using a function works.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

; CHECK-LABEL: inc:
define i32 @inc(i32 %a) {
entry:
  %0 = add nsw i32 %a, 1
  ret i32 %0
}


; CHECK-LABEL: main:
define i32 @main(i32 %value)  {
entry:
  %call = call i32 @inc(i32 %value)
  %call1 = call i32 @inc(i32 %call)

  ret i32 %call1
}

