; RUN: %seadsa %s --sea-dsa-dot --sea-dsa-type-aware=true --sea-dsa-dot-print-as --sea-dsa-dot-outdir=%T/atomic_cas.ll
; RUN: %cmp-graphs %tests/atomic_cas.ll.dot %T/atomic_cas.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$


; ModuleID = 'atomic_cas.c'
source_filename = "atomic_cas.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

; Function Attrs: noinline nounwind ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32*, align 8
  %3 = alloca i32, align 4
  %4 = alloca i32*, align 8
  %5 = alloca i32*, align 8
  %6 = alloca i8, align 1
  store i32 0, i32* %1, align 4
  store i32* null, i32** %2, align 8
  store i32 0, i32* %3, align 4
  %7 = load i32*, i32** %2, align 8
  store i32* %7, i32** %4, align 8
  store i32* %3, i32** %5, align 8
  %8 = bitcast i32** %4 to i64*
  %9 = bitcast i32** %2 to i64*
  %10 = bitcast i32** %5 to i64*
  %11 = load i64, i64* %9, align 8
  %12 = load i64, i64* %10, align 8
  %13 = cmpxchg weak i64* %8, i64 %11, i64 %12 monotonic monotonic
  %14 = extractvalue { i64, i1 } %13, 0
  %15 = extractvalue { i64, i1 } %13, 1
  br i1 %15, label %17, label %16

; <label>:16:                                     ; preds = %0
  store i64 %14, i64* %9, align 8
  br label %17

; <label>:17:                                     ; preds = %16, %0
  %18 = zext i1 %15 to i8
  store i8 %18, i8* %6, align 1
  %19 = load i8, i8* %6, align 1
  %20 = trunc i8 %19 to i1
  %21 = load i32*, i32** %4, align 8
  %22 = load i32, i32* %21, align 4
  %23 = load i32, i32* %3, align 4
  %24 = icmp eq i32 %22, %23
  call void @__VERIFIER_assert(i1 zeroext %24)
  ret i32 0
}

declare void @__VERIFIER_assert(i1 zeroext) #1

attributes #0 = { noinline nounwind ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
