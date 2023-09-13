; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_10.ll
; RUN: %cmp-graphs %tests/complete_callgraph_10.dot %T/complete_callgraph_10.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_10.c'
source_filename = "complete_callgraph_10.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

@arith_functions = global [4 x i32 (i32, i32)*] [i32 (i32, i32)* @add, i32 (i32, i32)* @sub, i32 (i32, i32)* @mul, i32 (i32, i32)* @div], align 8

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
  store i32 0, i32* %1, align 4
  store i32 0, i32* %2, align 4
  %5 = call i32 bitcast (i32 (...)* @nd_uint to i32 ()*)()
  store i32 %5, i32* %3, align 4
  %6 = call i32 bitcast (i32 (...)* @nd_uint to i32 ()*)()
  store i32 %6, i32* %4, align 4
  %7 = call i32 bitcast (i32 (...)* @nd_uint to i32 ()*)()
  switch i32 %7, label %28 [
    i32 0, label %8
    i32 1, label %13
    i32 2, label %18
    i32 3, label %23
  ]

8:                                                ; preds = %0
  %9 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 0), align 8
  %10 = load i32, i32* %3, align 4
  %11 = load i32, i32* %4, align 4
  %12 = call i32 %9(i32 %10, i32 %11)
  store i32 %12, i32* %2, align 4
  br label %29

13:                                               ; preds = %0
  %14 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 1), align 8
  %15 = load i32, i32* %3, align 4
  %16 = load i32, i32* %4, align 4
  %17 = call i32 %14(i32 %15, i32 %16)
  store i32 %17, i32* %2, align 4
  br label %29

18:                                               ; preds = %0
  %19 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 2), align 8
  %20 = load i32, i32* %3, align 4
  %21 = load i32, i32* %4, align 4
  %22 = call i32 %19(i32 %20, i32 %21)
  store i32 %22, i32* %2, align 4
  br label %29

23:                                               ; preds = %0
  %24 = load i32 (i32, i32)*, i32 (i32, i32)** getelementptr inbounds ([4 x i32 (i32, i32)*], [4 x i32 (i32, i32)*]* @arith_functions, i64 0, i64 3), align 8
  %25 = load i32, i32* %3, align 4
  %26 = load i32, i32* %4, align 4
  %27 = call i32 %24(i32 %25, i32 %26)
  store i32 %27, i32* %2, align 4
  br label %29

28:                                               ; preds = %0
  br label %29

29:                                               ; preds = %28, %23, %18, %13, %8
  %30 = load i32, i32* %2, align 4
  ret i32 %30
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
