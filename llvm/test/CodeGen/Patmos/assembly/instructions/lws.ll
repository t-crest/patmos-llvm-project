; RUN: LLC_ARGS="-mpatmos-enable-stack-cache-promotion"; \
; RUN: %test_no_runtime_execution

define i32 @main() {
entry:    
  %stack_var = alloca i32, align 4
  store i32 15, i32* %stack_var, align 4
  %stack_var_2 = alloca i32, align 4
  store i32 16, i32* %stack_var_2, align 4

  ; Extract results
  %result_1 = call i32 asm "lws $0 = [$1]", "=r,r" (i32* %stack_var)
  %result_2 = call i32 asm "lws $0 = [$1]
  nop", "=r,r" (i32* %stack_var_2)
	

  ; Check correctness
  %correct_1 = icmp eq i32 %result_1, 15
  %correct_2 = icmp eq i32 %result_2, 16
  %correct = and i1 %correct_1, %correct_2
  
  ; Negate result to ensure 0 is returned on success
  %result_0 = xor i1 %correct, 1 
  
  %result = zext i1 %result_0 to i32
  ret i32 %result
}