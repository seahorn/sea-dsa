; Cross-node unification with delta alignment: cells (A,4) and (B,8)
; are unified; node A is shifted by 4 and merged into B keeping all
; fields distinct — no collapse with or without partial collapse.
; RUN: %seadsa %s %butd_dsa --sea-dsa-type-aware=true --sea-dsa-partial-collapse --sea-dsa-dot --sea-dsa-dot-outdir=%T/cn.on
; RUN: cat %T/cn.on/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=ON
; RUN: cat %T/cn.on/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=NOCOLL
; ON: (?=.*0:i32)(?=.*4:i32).*8:i32
; NOCOLL-NOT: OFFSET-COLLAPSED

; ModuleID = 'tests/c/idsa_cross_node.c'
source_filename = "tests/c/idsa_cross_node.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.A = type { i32, i32 }
%struct.B = type { i32, i32, i32 }

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  %4 = alloca ptr, align 8
  store i32 0, ptr %1, align 4
  %5 = call noalias ptr @malloc(i64 noundef 8) #3
  store ptr %5, ptr %2, align 8
  %6 = call noalias ptr @malloc(i64 noundef 12) #3
  store ptr %6, ptr %3, align 8
  %7 = call i32 @nd_int()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %9, label %12

9:                                                ; preds = %0
  %10 = load ptr, ptr %2, align 8
  %11 = getelementptr inbounds %struct.A, ptr %10, i32 0, i32 1
  br label %15

12:                                               ; preds = %0
  %13 = load ptr, ptr %3, align 8
  %14 = getelementptr inbounds %struct.B, ptr %13, i32 0, i32 2
  br label %15

15:                                               ; preds = %12, %9
  %16 = phi ptr [ %11, %9 ], [ %14, %12 ]
  store ptr %16, ptr %4, align 8
  %17 = load ptr, ptr %4, align 8
  store i32 7, ptr %17, align 4
  %18 = load ptr, ptr %2, align 8
  %19 = getelementptr inbounds %struct.A, ptr %18, i32 0, i32 0
  %20 = load i32, ptr %19, align 4
  %21 = load ptr, ptr %3, align 8
  %22 = getelementptr inbounds %struct.B, ptr %21, i32 0, i32 0
  %23 = load i32, ptr %22, align 4
  %24 = add nsw i32 %20, %23
  ret i32 %24
}

; Function Attrs: nounwind allocsize(0)
declare noalias ptr @malloc(i64 noundef) #1

declare i32 @nd_int() #2

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind allocsize(0) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind allocsize(0) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 18.1.8 (++20240731024944+3b5b5c1ec4a3-1~exp1~20240731145000.144)"}
