; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_11.ll
; RUN: %cmp-graphs %tests/complete_callgraph_11.dot %T/complete_callgraph_11.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_11.bc'
source_filename = "complete_callgraph_11.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

@arith_functions = common global [4 x i32 (i32, i32)*] zeroinitializer, align 16

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
define i32 @mul(i32, i32) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  store i32 %1, i32* %4, align 4
  %5 = load i32, i32* %3, align 4
  %6 = load i32, i32* %4, align 4
  %7 = mul nsw i32 %5, %6
  ret i32 %7
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @div(i32, i32) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  store i32 %1, i32* %4, align 4
  %5 = load i32, i32* %3, align 4
  %6 = load i32, i32* %4, align 4
  %7 = sdiv i32 %5, %6
  ret i32 %7
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 (i32, i32)* @add, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 0), align 16
  store i32 (i32, i32)* @sub, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 1), align 8
  store i32 (i32, i32)* @mul, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 2), align 16
  store i32 (i32, i32)* @div, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 3), align 8
  store i32 0, i32* %2, align 4
  %5 = call i32 (...) @nd_uint()
  store i32 %5, i32* %3, align 4
  %6 = call i32 (...) @nd_uint()
  store i32 %6, i32* %4, align 4
  %7 = call i32 (...) @nd_uint()
  switch i32 %7, label %28 [
    i32 0, label %8
    i32 1, label %13
    i32 2, label %18
    i32 3, label %23
  ]

; <label>:8:                                      ; preds = %0
  %9 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 0), align 16
  %10 = load i32, i32* %3, align 4
  %11 = load i32, i32* %4, align 4
  %12 = call i32 %9(i32 %10, i32 %11)
  store i32 %12, i32* %2, align 4
  br label %29

; <label>:13:                                     ; preds = %0
  %14 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 1), align 8
  %15 = load i32, i32* %3, align 4
  %16 = load i32, i32* %4, align 4
  %17 = call i32 %14(i32 %15, i32 %16)
  store i32 %17, i32* %2, align 4
  br label %29

; <label>:18:                                     ; preds = %0
  %19 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 2), align 16
  %20 = load i32, i32* %3, align 4
  %21 = load i32, i32* %4, align 4
  %22 = call i32 %19(i32 %20, i32 %21)
  store i32 %22, i32* %2, align 4
  br label %29

; <label>:23:                                     ; preds = %0
  %24 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 3), align 8
  %25 = load i32, i32* %3, align 4
  %26 = load i32, i32* %4, align 4
  %27 = call i32 %24(i32 %25, i32 %26)
  store i32 %27, i32* %2, align 4
  br label %29

; <label>:28:                                     ; preds = %0
  br label %29

; <label>:29:                                     ; preds = %28, %23, %18, %13, %8
  %30 = load i32, i32* %2, align 4
  ret i32 %30
}

declare i32 @nd_uint(...) #1

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
