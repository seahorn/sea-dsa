; RUN: %seadsa %s %butd_dsa --sea-dsa-dot --sea-dsa-type-aware=true --sea-dsa-dot-outdir=%T
; RUN: %cmp-graphs %tests/simple.dot %T/main.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'tests/c/simple.c'
source_filename = "tests/c/simple.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

%struct.S = type { i32**, i32** }

@g = common global i32 0, align 4

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main(i32, i8**) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 8
  %6 = alloca %struct.S, align 8
  %7 = alloca %struct.S, align 8
  %8 = alloca i32*, align 8
  %9 = alloca i32*, align 8
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4
  store i8** %1, i8*** %5, align 8
  %10 = call i8* @malloc(i64 4) #2
  %11 = bitcast i8* %10 to i32*
  store i32* %11, i32** %8, align 8
  %12 = call i8* @malloc(i64 4) #2
  %13 = bitcast i8* %12 to i32*
  store i32* %13, i32** %9, align 8
  %14 = getelementptr inbounds %struct.S, %struct.S* %6, i32 0, i32 0
  store i32** %8, i32*** %14, align 8
  %15 = getelementptr inbounds %struct.S, %struct.S* %6, i32 0, i32 1
  store i32** %9, i32*** %15, align 8
  %16 = getelementptr inbounds %struct.S, %struct.S* %6, i32 0, i32 0
  %17 = load i32**, i32*** %16, align 8
  store i32* @g, i32** %17, align 8
  ret i32 0
}

; Function Attrs: allocsize(0)
declare i8* @malloc(i64) #1

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { allocsize(0) "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { allocsize(0) }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
