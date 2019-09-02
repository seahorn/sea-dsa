; RUN: %seadsa %s --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_13.ll
; RUN: %cmp-graphs %tests/complete_callgraph_13.dot %T/complete_callgraph_13.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_13.bc'
source_filename = "complete_callgraph_13.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @add(i32, i32) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  store i32 %1, i32* %4, align 4
  %5 = load i32, i32* %3, align 4
  %6 = load i32, i32* %4, align 4
  %7 = add nsw i32 %5, %6
  ret i32 %7
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @sub(i32, i32) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  store i32 %1, i32* %4, align 4
  %5 = load i32, i32* %3, align 4
  %6 = load i32, i32* %4, align 4
  %7 = sub nsw i32 %5, %6
  ret i32 %7
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @apply(i32 (i32, i32)*, i32, i32) #0 {
  %4 = alloca i32 (i32, i32)*, align 8
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  store i32 (i32, i32)* %0, i32 (i32, i32)** %4, align 8
  store i32 %1, i32* %5, align 4
  store i32 %2, i32* %6, align 4
  %7 = load i32 (i32, i32)*, i32 (i32, i32)** %4, align 8
  %8 = load i32, i32* %5, align 4
  %9 = load i32, i32* %6, align 4
  %10 = call i32 %7(i32 %8, i32 %9)
  ret i32 %10
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 (i32, i32)* @bar(i32 (i32, i32)*) #0 {
  %2 = alloca i32 (i32, i32)*, align 8
  store i32 (i32, i32)* %0, i32 (i32, i32)** %2, align 8
  %3 = load i32 (i32, i32)*, i32 (i32, i32)** %2, align 8
  ret i32 (i32, i32)* %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @foo(i32 (i32, i32)*, i32, i32) #0 {
  %4 = alloca i32 (i32, i32)*, align 8
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = alloca i32 (i32, i32)*, align 8
  store i32 (i32, i32)* %0, i32 (i32, i32)** %4, align 8
  store i32 %1, i32* %5, align 4
  store i32 %2, i32* %6, align 4
  %8 = load i32 (i32, i32)*, i32 (i32, i32)** %4, align 8
  %9 = call i32 (i32, i32)* @bar(i32 (i32, i32)* %8)
  store i32 (i32, i32)* %9, i32 (i32, i32)** %7, align 8
  %10 = load i32 (i32, i32)*, i32 (i32, i32)** %7, align 8
  %11 = load i32, i32* %5, align 4
  %12 = load i32, i32* %6, align 4
  %13 = call i32 @apply(i32 (i32, i32)* %10, i32 %11, i32 %12)
  ret i32 %13
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %4 = call i32 @foo(i32 (i32, i32)* @add, i32 2, i32 5)
  store i32 %4, i32* %2, align 4
  %5 = call i32 (i32, i32)* (...) @nd_binfptr()
  %6 = call i32 @foo(i32 (i32, i32)* %5, i32 5, i32 7)
  store i32 %6, i32* %3, align 4
  %7 = load i32, i32* %2, align 4
  %8 = load i32, i32* %3, align 4
  %9 = add nsw i32 %7, %8
  ret i32 %9
}

declare i32 (i32, i32)* @nd_binfptr(...) #1

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
