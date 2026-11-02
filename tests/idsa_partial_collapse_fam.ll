; Flexible-array-member struct (paper overview example): with partial
; collapse the scalar fields keep distinct cells and the symbolically
; indexed array becomes the interval [12,+oo); without the flag the
; whole node offset-collapses.
; RUN: %seadsa %s %butd_dsa --sea-dsa-type-aware=true --sea-dsa-partial-collapse --sea-dsa-dot --sea-dsa-dot-outdir=%T/fam.on
; RUN: cat %T/fam.on/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=ON
; RUN: cat %T/fam.on/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=NOCOLL
; RUN: %seadsa %s %butd_dsa --sea-dsa-type-aware=true --sea-dsa-dot --sea-dsa-dot-outdir=%T/fam.off
; RUN: cat %T/fam.off/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=OFF
; ON: gold2.*(?=.*0:i32)(?=.*4:i32)(?=.*8:i32).*\[12-\+oo\]:raw
; NOCOLL-NOT: OFFSET-COLLAPSED
; OFF: OFFSET-COLLAPSED

; ModuleID = 'tests/c/idsa_fam.c'
source_filename = "tests/c/idsa_fam.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.rb = type { i32, i32, i32, [0 x i8] }

@g_id = dso_local global i32 0, align 4

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca ptr, align 8
  %4 = alloca i32, align 4
  store i32 0, ptr %1, align 4
  %5 = call i32 @nd_int()
  store i32 %5, ptr %2, align 4
  %6 = load i32, ptr %2, align 4
  %7 = icmp sle i32 %6, 0
  br i1 %7, label %8, label %9

8:                                                ; preds = %0
  store i32 0, ptr %1, align 4
  br label %41

9:                                                ; preds = %0
  %10 = load i32, ptr %2, align 4
  %11 = sext i32 %10 to i64
  %12 = add i64 12, %11
  %13 = call noalias ptr @malloc(i64 noundef %12) #3
  store ptr %13, ptr %3, align 8
  %14 = load i32, ptr @g_id, align 4
  %15 = add nsw i32 %14, 1
  store i32 %15, ptr @g_id, align 4
  %16 = load ptr, ptr %3, align 8
  %17 = getelementptr inbounds %struct.rb, ptr %16, i32 0, i32 0
  store i32 %14, ptr %17, align 4
  %18 = load i32, ptr %2, align 4
  %19 = load ptr, ptr %3, align 8
  %20 = getelementptr inbounds %struct.rb, ptr %19, i32 0, i32 1
  store i32 %18, ptr %20, align 4
  %21 = load ptr, ptr %3, align 8
  %22 = getelementptr inbounds %struct.rb, ptr %21, i32 0, i32 2
  store i32 0, ptr %22, align 4
  store i32 0, ptr %4, align 4
  br label %23

23:                                               ; preds = %34, %9
  %24 = load i32, ptr %4, align 4
  %25 = load i32, ptr %2, align 4
  %26 = icmp slt i32 %24, %25
  br i1 %26, label %27, label %37

27:                                               ; preds = %23
  %28 = call signext i8 @nd_char()
  %29 = load ptr, ptr %3, align 8
  %30 = getelementptr inbounds %struct.rb, ptr %29, i32 0, i32 3
  %31 = load i32, ptr %4, align 4
  %32 = sext i32 %31 to i64
  %33 = getelementptr inbounds [0 x i8], ptr %30, i64 0, i64 %32
  store i8 %28, ptr %33, align 1
  br label %34

34:                                               ; preds = %27
  %35 = load i32, ptr %4, align 4
  %36 = add nsw i32 %35, 1
  store i32 %36, ptr %4, align 4
  br label %23, !llvm.loop !6

37:                                               ; preds = %23
  %38 = load ptr, ptr %3, align 8
  %39 = getelementptr inbounds %struct.rb, ptr %38, i32 0, i32 0
  %40 = load i32, ptr %39, align 4
  store i32 %40, ptr %1, align 4
  br label %41

41:                                               ; preds = %37, %8
  %42 = load i32, ptr %1, align 4
  ret i32 %42
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
