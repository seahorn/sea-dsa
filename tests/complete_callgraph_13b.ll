; RUN: %seadsa %s --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_13b.ll
; RUN: %cmp-graphs %tests/complete_callgraph_13b.dot %T/complete_callgraph_13b.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_13b.c'
source_filename = "complete_callgraph_13b.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

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
define i32 @apply(i32 (i32, i32)* %0, i32 %1, i32 %2) #0 {
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
define i32 (i32, i32)* @bar(i32 (i32, i32)* %0) #0 {
  %2 = alloca i32 (i32, i32)*, align 8
  store i32 (i32, i32)* %0, i32 (i32, i32)** %2, align 8
  %3 = load i32 (i32, i32)*, i32 (i32, i32)** %2, align 8
  ret i32 (i32, i32)* %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @foo(i32 (i32, i32)* %0, i32 %1, i32 %2) #0 {
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
  %5 = call noalias i32 (i32, i32)* @nd_binfptr()
  %6 = call i32 @foo(i32 (i32, i32)* %5, i32 5, i32 7)
  store i32 %6, i32* %3, align 4
  %7 = load i32, i32* %2, align 4
  %8 = load i32, i32* %3, align 4
  %9 = add nsw i32 %7, %8
  ret i32 %9
}

declare noalias i32 (i32, i32)* @nd_binfptr() #1

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
