; Degeneration: joining a cell at offset 0 with the unbounded interval
; of a flexible array yields [0,+oo); partial collapse then soundly
; degenerates to a full offset collapse even with the flag enabled.
; RUN: %seadsa %s %butd_dsa --sea-dsa-type-aware=true --sea-dsa-partial-collapse --sea-dsa-dot --sea-dsa-dot-outdir=%T/dg.on
; RUN: cat %T/dg.on/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=ON
; ON: OFFSET-COLLAPSED

; ModuleID = 'tests/c/idsa_degenerate.c'
source_filename = "tests/c/idsa_degenerate.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.rb = type { i32, i32, [0 x i8] }

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca ptr, align 8
  %4 = alloca i32, align 4
  %5 = alloca ptr, align 8
  store i32 0, ptr %1, align 4
  %6 = call i32 @nd_int()
  store i32 %6, ptr %2, align 4
  %7 = load i32, ptr %2, align 4
  %8 = icmp sle i32 %7, 1
  br i1 %8, label %9, label %10

9:                                                ; preds = %0
  store i32 0, ptr %1, align 4
  br label %51

10:                                               ; preds = %0
  %11 = load i32, ptr %2, align 4
  %12 = sext i32 %11 to i64
  %13 = add i64 8, %12
  %14 = call noalias ptr @malloc(i64 noundef %13) #3
  store ptr %14, ptr %3, align 8
  %15 = load ptr, ptr %3, align 8
  %16 = getelementptr inbounds %struct.rb, ptr %15, i32 0, i32 0
  store i32 1, ptr %16, align 4
  %17 = load i32, ptr %2, align 4
  %18 = load ptr, ptr %3, align 8
  %19 = getelementptr inbounds %struct.rb, ptr %18, i32 0, i32 1
  store i32 %17, ptr %19, align 4
  store i32 0, ptr %4, align 4
  br label %20

20:                                               ; preds = %31, %10
  %21 = load i32, ptr %4, align 4
  %22 = load i32, ptr %2, align 4
  %23 = icmp slt i32 %21, %22
  br i1 %23, label %24, label %34

24:                                               ; preds = %20
  %25 = call signext i8 @nd_char()
  %26 = load ptr, ptr %3, align 8
  %27 = getelementptr inbounds %struct.rb, ptr %26, i32 0, i32 2
  %28 = load i32, ptr %4, align 4
  %29 = sext i32 %28 to i64
  %30 = getelementptr inbounds [0 x i8], ptr %27, i64 0, i64 %29
  store i8 %25, ptr %30, align 1
  br label %31

31:                                               ; preds = %24
  %32 = load i32, ptr %4, align 4
  %33 = add nsw i32 %32, 1
  store i32 %33, ptr %4, align 4
  br label %20, !llvm.loop !6

34:                                               ; preds = %20
  %35 = call i32 @nd_int()
  %36 = icmp ne i32 %35, 0
  br i1 %36, label %37, label %40

37:                                               ; preds = %34
  %38 = load ptr, ptr %3, align 8
  %39 = getelementptr inbounds %struct.rb, ptr %38, i32 0, i32 0
  br label %44

40:                                               ; preds = %34
  %41 = load ptr, ptr %3, align 8
  %42 = getelementptr inbounds %struct.rb, ptr %41, i32 0, i32 2
  %43 = getelementptr inbounds [0 x i8], ptr %42, i64 0, i64 0
  br label %44

44:                                               ; preds = %40, %37
  %45 = phi ptr [ %39, %37 ], [ %43, %40 ]
  store ptr %45, ptr %5, align 8
  %46 = call signext i8 @nd_char()
  %47 = load ptr, ptr %5, align 8
  store i8 %46, ptr %47, align 1
  %48 = load ptr, ptr %3, align 8
  %49 = getelementptr inbounds %struct.rb, ptr %48, i32 0, i32 0
  %50 = load i32, ptr %49, align 4
  store i32 %50, ptr %1, align 4
  br label %51

51:                                               ; preds = %44, %9
  %52 = load i32, ptr %1, align 4
  ret i32 %52
}

declare i32 @nd_int() #1

; Function Attrs: nounwind allocsize(0)
declare noalias ptr @malloc(i64 noundef) #2

declare signext i8 @nd_char() #1

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nounwind allocsize(0) "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { nounwind allocsize(0) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 18.1.8 (++20240731024944+3b5b5c1ec4a3-1~exp1~20240731145000.144)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
