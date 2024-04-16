;//////////////////////////////////////////////////////////////////////////////////////////////////
;
; Tests that summing two members of a struct is correct
;
;//////////////////////////////////////////////////////////////////////////////////////////////////


%struct.t = type { i32, i32 }

@__const.main.a = private unnamed_addr constant %struct.t { i32 5, i32 5 }, align 4

; Function Attrs: noinline nounwind optnone
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %a = alloca %struct.t, align 4
  %x = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %0 = bitcast %struct.t* %a to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* align 4 %0, i8* align 4 bitcast (%struct.t* @__const.main.a to i8*), i32 8, i1 false)
  store i32 0, i32* %x, align 4
  %a1 = getelementptr inbounds %struct.t, %struct.t* %a, i32 0, i32 0
  %1 = load i32, i32* %a1, align 4
  %2 = load i32, i32* %x, align 4
  %add = add nsw i32 %2, %1
  store i32 %add, i32* %x, align 4
  %b = getelementptr inbounds %struct.t, %struct.t* %a, i32 0, i32 1
  %3 = load i32, i32* %b, align 4
  %4 = load i32, i32* %x, align 4
  %add2 = add nsw i32 %4, %3
  store i32 %add2, i32* %x, align 4
  %5 = load i32, i32* %x, align 4
  ret i32 %5
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i32(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i32, i1 immarg) #1

attributes #0 = { noinline nounwind optnone "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }