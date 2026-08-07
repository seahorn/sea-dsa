; RUN: %seadsa %s --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_14.ll
; RUN: %cmp-graphs %tests/complete_callgraph_14.dot %T/complete_callgraph_14.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$


; ModuleID = 'complete_callgraph_14.c'
source_filename = "complete_callgraph_14.c"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%struct.Struct = type { i32, i32, ptr }

@hash = common global ptr null, align 8
@hash_ptr = global ptr @hash, align 8

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @jenkins_hash(i32 %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  ret i32 %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @MurmurHash3_hash(i32 %0) #0 {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  ret i32 %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @hash_init() #0 {
  %1 = call i32 @nd()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %3, label %5

3:                                                ; preds = %0
  %4 = load ptr, ptr @hash_ptr, align 8
  store ptr @jenkins_hash, ptr %4, align 8
  br label %7

5:                                                ; preds = %0
  %6 = load ptr, ptr @hash_ptr, align 8
  store ptr @MurmurHash3_hash, ptr %6, align 8
  br label %7

7:                                                ; preds = %5, %3
  ret void
}

declare i32 @nd(...) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @init(ptr %0, ptr %1) #0 {
  %3 = alloca ptr, align 8
  %4 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
  store ptr %1, ptr %4, align 8
  %5 = load ptr, ptr %4, align 8
  call void %5()
  %6 = load ptr, ptr @hash, align 8
  %7 = load ptr, ptr %3, align 8
  %8 = getelementptr inbounds %struct.Struct, ptr %7, i32 0, i32 2
  %9 = load ptr, ptr %8, align 8
  store ptr %6, ptr %9, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca ptr, align 8
  store i32 0, ptr %1, align 4
  %3 = call ptr @malloc(i64 16) #3
  %4 = bitcast ptr %3 to ptr
  store ptr %4, ptr %2, align 8
  %5 = load ptr, ptr %2, align 8
  %6 = getelementptr inbounds %struct.Struct, ptr %5, i32 0, i32 0
  store i32 0, ptr %6, align 8
  %7 = load ptr, ptr %2, align 8
  %8 = getelementptr inbounds %struct.Struct, ptr %7, i32 0, i32 1
  store i32 0, ptr %8, align 4
  %9 = call ptr @malloc(i64 8) #3
  %10 = bitcast ptr %9 to ptr
  %11 = load ptr, ptr %2, align 8
  %12 = getelementptr inbounds %struct.Struct, ptr %11, i32 0, i32 2
  store ptr %10, ptr %12, align 8
  %13 = load ptr, ptr %2, align 8
  call void @init(ptr %13, ptr @hash_init)
  %14 = load ptr, ptr %2, align 8
  %15 = getelementptr inbounds %struct.Struct, ptr %14, i32 0, i32 2
  %16 = load ptr, ptr %15, align 8
  %17 = load ptr, ptr %16, align 8
  %18 = call i32 %17(i32 8)
  ret i32 %18
}

; Function Attrs: allocsize(0)
declare ptr @malloc(i64) #2

attributes #0 = { noinline nounwind optnone ssp uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #1 = { "frame-pointer"="non-leaf" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #2 = { allocsize(0) "frame-pointer"="non-leaf" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #3 = { allocsize(0) }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 13, i32 1]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"branch-target-enforcement", i32 0}
!3 = !{i32 8, !"sign-return-address", i32 0}
!4 = !{i32 8, !"sign-return-address-all", i32 0}
!5 = !{i32 8, !"sign-return-address-with-bkey", i32 0}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 1}
!8 = !{i32 7, !"frame-pointer", i32 1}
!9 = !{!"Apple clang version 14.0.0 (clang-1400.0.29.202)"}
