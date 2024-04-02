; RUN: EXEC_ARGS="1=7 2=8"; \
; RUN: %test_execution
; END.
;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that summing two members of a struct is correct
;
;//////////////////////////////////////////////////////////////////////////////////////////////////


%struct.t = type { i32, i32 }

@__const.main.a = private unnamed_addr constant %struct.t { i32 5, i32 5 }, align 4

; Function Attrs: noinline nounwind optnone
define dso_local i32 @main(i32 %value) #0 {
entry:
  ; 0 = (struct.t) malloc(sizeof(struct.t));
  %a = alloca %struct.t, align 4

  ; x = (int) malloc(4);
  %x = alloca i32, align 4


  %tmpa = getelementptr inbounds %struct.t, %struct.t* %a, i32 0, i32 0
  store i32 %value, i32* %tmpa, align 4

  %tmpb = getelementptr inbounds %struct.t, %struct.t* %a, i32 0, i32 1
  store i32 6, i32* %tmpb, align 4

  ; x = 0;
  store i32 0, i32* %x, align 4

  ; x += a;
  %a1 = getelementptr inbounds %struct.t, %struct.t* %a, i32 0, i32 0
  %0 = load i32, i32* %a1, align 4
  %1 = load i32, i32* %x, align 4
  %add = add nsw i32 %1, %0

  ; x += b;
  store i32 %add, i32* %x, align 4
  %b = getelementptr inbounds %struct.t, %struct.t* %a, i32 0, i32 1
  %2 = load i32, i32* %b, align 4
  %3 = load i32, i32* %x, align 4
  %add2 = add nsw i32 %3, %2
  store i32 %add2, i32* %x, align 4

  ; return x;
  %4 = load i32, i32* %x, align 4
  ret i32 %4
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i32(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i32, i1 immarg) #1

attributes #0 = { noinline nounwind optnone "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }