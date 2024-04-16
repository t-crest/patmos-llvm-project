; RUN: EXEC_ARGS="1=2 2=3"; \
; RUN: WITH_DEBUG=true; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that incrementing twice using a function works.
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @inc(i32 %a) {
entry:
  %0 = add nsw i32 %a, 1
  ret i32 %0
}

define i32 @inc1(i32 %a) {
entry:
  %0 = call i32 @inc(i32 %a)

  ret i32 %0
}

define i32 @inc2(i32 %a) {
entry:
  %0 = call i32 @inc1(i32 %a);

  ret i32 %0
}


; CHECK-LABEL: main:
define i32 @main(i32 %value)  {
entry:
  %call = call i32 @inc2(i32 %value)

  ret i32 %call
}

