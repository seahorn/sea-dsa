; RUN: %seadsa %s --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_12.ll
; RUN: %cmp-graphs %tests/complete_callgraph_12.dot %T/complete_callgraph_12.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_12.bc'
source_filename = "complete_callgraph_12.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

%struct.Struct = type { i32*, i32 (i32, i32)* }

@arith_functions = global [4 x %struct.Struct] [%struct.Struct { i32* null, i32 (i32, i32)* @add }, %struct.Struct { i32* null, i32 (i32, i32)* @sub }, %struct.Struct { i32* null, i32 (i32, i32)* @mul }, %struct.Struct { i32* null, i32 (i32, i32)* @div }], align 16

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
  %5 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 789, i32* %2, align 4
  store i32* %2, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 0, i32 0), align 16
  store i32* %2, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 1, i32 0), align 16
  store i32* %2, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 2, i32 0), align 16
  store i32* %2, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 3, i32 0), align 16
  store i32 0, i32* %3, align 4
  %6 = call i32 (...) @nd_uint()
  store i32 %6, i32* %4, align 4
  %7 = call i32 (...) @nd_uint()
  store i32 %7, i32* %5, align 4
  %8 = call i32 (...) @nd_uint()
  switch i32 %8, label %41 [
    i32 0, label %9
    i32 1, label %17
    i32 2, label %25
    i32 3, label %33
  ]

; <label>:9:                                      ; preds = %0
  %10 = load i32*, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 0, i32 0), align 16
  %11 = load i32, i32* %10, align 4
  %12 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 0, i32 1), align 8
  %13 = load i32, i32* %4, align 4
  %14 = load i32, i32* %5, align 4
  %15 = call i32 %12(i32 %13, i32 %14)
  %16 = add nsw i32 %11, %15
  store i32 %16, i32* %3, align 4
  br label %42

; <label>:17:                                     ; preds = %0
  %18 = load i32*, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 1, i32 0), align 16
  %19 = load i32, i32* %18, align 4
  %20 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 1, i32 1), align 8
  %21 = load i32, i32* %4, align 4
  %22 = load i32, i32* %5, align 4
  %23 = call i32 %20(i32 %21, i32 %22)
  %24 = add nsw i32 %19, %23
  store i32 %24, i32* %3, align 4
  br label %42

; <label>:25:                                     ; preds = %0
  %26 = load i32*, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 2, i32 0), align 16
  %27 = load i32, i32* %26, align 4
  %28 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 2, i32 1), align 8
  %29 = load i32, i32* %4, align 4
  %30 = load i32, i32* %5, align 4
  %31 = call i32 %28(i32 %29, i32 %30)
  %32 = add nsw i32 %27, %31
  store i32 %32, i32* %3, align 4
  br label %42

; <label>:33:                                     ; preds = %0
  %34 = load i32*, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 3, i32 0), align 16
  %35 = load i32, i32* %34, align 4
  %36 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 3, i32 1), align 8
  %37 = load i32, i32* %4, align 4
  %38 = load i32, i32* %5, align 4
  %39 = call i32 %36(i32 %37, i32 %38)
  %40 = add nsw i32 %35, %39
  store i32 %40, i32* %3, align 4
  br label %42

; <label>:41:                                     ; preds = %0
  br label %42

; <label>:42:                                     ; preds = %41, %33, %25, %17, %9
  %43 = load i32, i32* %3, align 4
  ret i32 %43
}

declare i32 @nd_uint(...) #1

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
