; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_16.ll
; RUN: %cmp-graphs %tests/complete_callgraph_16.dot %T/complete_callgraph_16.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_16.c'
source_filename = "complete_callgraph_16.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.test_handler = type { %struct.test_sub, i32 (...)* }
%struct.test_sub = type { i32 }

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @foo() #0 {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = load i32, i32* %1, align 4
  %3 = icmp eq i32 %2, 0
  br i1 %3, label %5, label %4

4:                                                ; preds = %0
  call void @__VERIFIER_error()
  br label %5

5:                                                ; preds = %4, %0
  %6 = phi i1 [ true, %0 ], [ false, %4 ]
  %7 = zext i1 %6 to i32
  %8 = load i32, i32* %1, align 4
  ret i32 %8
}

declare void @__VERIFIER_error() #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define %struct.test_handler* @return_handler_struct() #0 {
  %1 = alloca %struct.test_handler*, align 8
  %2 = call i8* @malloc(i64 16) #3
  %3 = bitcast i8* %2 to %struct.test_handler*
  store %struct.test_handler* %3, %struct.test_handler** %1, align 8
  %4 = load %struct.test_handler*, %struct.test_handler** %1, align 8
  %5 = getelementptr inbounds %struct.test_handler, %struct.test_handler* %4, i32 0, i32 1
  store i32 (...)* bitcast (i32 ()* @foo to i32 (...)*), i32 (...)** %5, align 8
  %6 = load %struct.test_handler*, %struct.test_handler** %1, align 8
  ret %struct.test_handler* %6
}

; Function Attrs: allocsize(0)
declare i8* @malloc(i64) #2

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main(i32 %0, i8** %1) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 8
  %6 = alloca %struct.test_handler*, align 8
  %7 = alloca %struct.test_sub*, align 8
  %8 = alloca %struct.test_handler*, align 8
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4
  store i8** %1, i8*** %5, align 8
  %9 = call %struct.test_handler* @return_handler_struct()
  store %struct.test_handler* %9, %struct.test_handler** %6, align 8
  %10 = load %struct.test_handler*, %struct.test_handler** %6, align 8
  %11 = bitcast %struct.test_handler* %10 to %struct.test_sub*
  store %struct.test_sub* %11, %struct.test_sub** %7, align 8
  %12 = load %struct.test_sub*, %struct.test_sub** %7, align 8
  %13 = call %struct.test_handler* @to_test_handler(%struct.test_sub* %12)
  store %struct.test_handler* %13, %struct.test_handler** %8, align 8
  %14 = load %struct.test_handler*, %struct.test_handler** %8, align 8
  %15 = getelementptr inbounds %struct.test_handler, %struct.test_handler* %14, i32 0, i32 1
  %16 = load i32 (...)*, i32 (...)** %15, align 8
  %17 = bitcast i32 (...)* %16 to i32 ()*
  %18 = call i32 %17()
  %19 = icmp eq i32 %18, 0
  br i1 %19, label %21, label %20

20:                                               ; preds = %2
  call void @__VERIFIER_error()
  br label %21

21:                                               ; preds = %20, %2
  %22 = phi i1 [ true, %2 ], [ false, %20 ]
  %23 = zext i1 %22 to i32
  ret i32 0
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define internal %struct.test_handler* @to_test_handler(%struct.test_sub* %0) #0 {
  %2 = alloca %struct.test_sub*, align 8
  store %struct.test_sub* %0, %struct.test_sub** %2, align 8
  %3 = load %struct.test_sub*, %struct.test_sub** %2, align 8
  %4 = icmp ne %struct.test_sub* %3, null
  br i1 %4, label %6, label %5

5:                                                ; preds = %1
  call void @__VERIFIER_error()
  br label %6

6:                                                ; preds = %5, %1
  %7 = phi i1 [ true, %1 ], [ false, %5 ]
  %8 = zext i1 %7 to i32
  %9 = load %struct.test_sub*, %struct.test_sub** %2, align 8
  %10 = ptrtoint %struct.test_sub* %9 to i64
  %11 = sub i64 %10, 0
  %12 = inttoptr i64 %11 to %struct.test_handler*
  ret %struct.test_handler* %12
}

attributes #0 = { noinline nounwind optnone ssp uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #1 = { "frame-pointer"="non-leaf" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #2 = { allocsize(0) "frame-pointer"="non-leaf" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #3 = { allocsize(0) }

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
