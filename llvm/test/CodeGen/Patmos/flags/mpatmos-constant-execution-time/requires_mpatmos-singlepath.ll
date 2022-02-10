; RUN: llc < %s -mpatmos-enable-constant-execution-time=true %XFAIL-filecheck %s
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that if the 'mpatmos-enable-constant-execution-time' is given, 
; but 'mpatmos-singlepath' is not, an error is thrown
;
;//////////////////////////////////////////////////////////////////////////////////////////////////

define i32 @main() {
entry:
  ret i32 0
}

; CHECK: The 'mpatmos-enable-constant-execution-time' flag requires the 'mpatmos-singlepath' flag to also be set.
