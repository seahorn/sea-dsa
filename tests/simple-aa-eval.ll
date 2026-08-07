; RUN: %seadsa --sea-dsa-aa-eval --sea-dsa-type-aware=true %s 2>&1 | OutputCheck %s -d --comment=";"
; CHECK: ^===== Alias Analysis Evaluator Report =====$

; ModuleID = 'tests/c/simple.c'
source_filename = "tests/c/simple.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

%struct.S = type { ptr, ptr }

@g = common global i32 0, align 4

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main(i32 %0, ptr %1) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca ptr, align 8
  %6 = alloca %struct.S, align 8
  %7 = alloca %struct.S, align 8
  %8 = alloca ptr, align 8
  %9 = alloca ptr, align 8
  store i32 0, ptr %3, align 4
  store i32 %0, ptr %4, align 4
  store ptr %1, ptr %5, align 8
  %10 = call ptr @malloc(i64 4) #2
  %11 = bitcast ptr %10 to ptr
  store ptr %11, ptr %8, align 8
  %12 = call ptr @malloc(i64 4) #2
  %13 = bitcast ptr %12 to ptr
  store ptr %13, ptr %9, align 8
  %14 = getelementptr inbounds %struct.S, ptr %6, i32 0, i32 0
  store ptr %8, ptr %14, align 8
  %15 = getelementptr inbounds %struct.S, ptr %6, i32 0, i32 1
  store ptr %9, ptr %15, align 8
  %16 = getelementptr inbounds %struct.S, ptr %6, i32 0, i32 0
  %17 = load ptr, ptr %16, align 8
  store ptr @g, ptr %17, align 8
  ret i32 0
}

; Function Attrs: allocsize(0)
declare ptr @malloc(i64) #1

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { allocsize(0) "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { allocsize(0) }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
