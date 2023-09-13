; RUN: %seadsa %s --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_12.ll
; RUN: %cmp-graphs %tests/complete_callgraph_12.dot %T/complete_callgraph_12.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$


; ModuleID = 'complete_callgraph_12.c'
source_filename = "complete_callgraph_12.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.Struct = type { i32*, i32 (i32, i32)* }

@arith_functions = global [4 x %struct.Struct] [%struct.Struct { i32* null, i32 (i32, i32)* @add }, %struct.Struct { i32* null, i32 (i32, i32)* @sub }, %struct.Struct { i32* null, i32 (i32, i32)* @mul }, %struct.Struct { i32* null, i32 (i32, i32)* @div }], align 8

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @add(i32 %0, i32 %1) #0 {
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
define i32 @sub(i32 %0, i32 %1) #0 {
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
define i32 @mul(i32 %0, i32 %1) #0 {
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
define i32 @div(i32 %0, i32 %1) #0 {
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
  store i32* %2, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 0, i32 0), align 8
  store i32* %2, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 1, i32 0), align 8
  store i32* %2, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 2, i32 0), align 8
  store i32* %2, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 3, i32 0), align 8
  store i32 0, i32* %3, align 4
  %6 = call i32 bitcast (i32 (...)* @nd_uint to i32 ()*)()
  store i32 %6, i32* %4, align 4
  %7 = call i32 bitcast (i32 (...)* @nd_uint to i32 ()*)()
  store i32 %7, i32* %5, align 4
  %8 = call i32 bitcast (i32 (...)* @nd_uint to i32 ()*)()
  switch i32 %8, label %41 [
    i32 0, label %9
    i32 1, label %17
    i32 2, label %25
    i32 3, label %33
  ]

9:                                                ; preds = %0
  %10 = load i32*, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 0, i32 0), align 8
  %11 = load i32, i32* %10, align 4
  %12 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 0, i32 1), align 8
  %13 = load i32, i32* %4, align 4
  %14 = load i32, i32* %5, align 4
  %15 = call i32 %12(i32 %13, i32 %14)
  %16 = add nsw i32 %11, %15
  store i32 %16, i32* %3, align 4
  br label %42

17:                                               ; preds = %0
  %18 = load i32*, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 1, i32 0), align 8
  %19 = load i32, i32* %18, align 4
  %20 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 1, i32 1), align 8
  %21 = load i32, i32* %4, align 4
  %22 = load i32, i32* %5, align 4
  %23 = call i32 %20(i32 %21, i32 %22)
  %24 = add nsw i32 %19, %23
  store i32 %24, i32* %3, align 4
  br label %42

25:                                               ; preds = %0
  %26 = load i32*, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 2, i32 0), align 8
  %27 = load i32, i32* %26, align 4
  %28 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 2, i32 1), align 8
  %29 = load i32, i32* %4, align 4
  %30 = load i32, i32* %5, align 4
  %31 = call i32 %28(i32 %29, i32 %30)
  %32 = add nsw i32 %27, %31
  store i32 %32, i32* %3, align 4
  br label %42

33:                                               ; preds = %0
  %34 = load i32*, i32** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 3, i32 0), align 8
  %35 = load i32, i32* %34, align 4
  %36 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x %struct.Struct], [4 x %struct.Struct]* @arith_functions, i64 0, i64 3, i32 1), align 8
  %37 = load i32, i32* %4, align 4
  %38 = load i32, i32* %5, align 4
  %39 = call i32 %36(i32 %37, i32 %38)
  %40 = add nsw i32 %35, %39
  store i32 %40, i32* %3, align 4
  br label %42

41:                                               ; preds = %0
  br label %42

42:                                               ; preds = %41, %33, %25, %17, %9
  %43 = load i32, i32* %3, align 4
  ret i32 %43
}

declare i32 @nd_uint(...) #1

attributes #0 = { noinline nounwind optnone ssp uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #1 = { "frame-pointer"="non-leaf" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 13, i32 1]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 1, !"branch-target-enforcement", i32 0}
!3 = !{i32 1, !"sign-return-address", i32 0}
!4 = !{i32 1, !"sign-return-address-all", i32 0}
!5 = !{i32 1, !"sign-return-address-with-bkey", i32 0}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 1}
!8 = !{i32 7, !"frame-pointer", i32 1}
!9 = !{!"Apple clang version 14.0.0 (clang-1400.0.29.202)"}
