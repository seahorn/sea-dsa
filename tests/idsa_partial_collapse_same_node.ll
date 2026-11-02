; Same-node cell unification (paper if2ip example): one pointer may
; address fields at offsets 4 and 8 of the same object; the cells merge
; into the interval [4,8] while fields 0 and 12 stay distinct.
; RUN: %seadsa %s %butd_dsa --sea-dsa-type-aware=true --sea-dsa-partial-collapse --sea-dsa-dot --sea-dsa-dot-outdir=%T/sn.on
; RUN: cat %T/sn.on/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=ON
; RUN: %seadsa %s %butd_dsa --sea-dsa-type-aware=true --sea-dsa-dot --sea-dsa-dot-outdir=%T/sn.off
; RUN: cat %T/sn.off/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=OFF
; ON: gold2.*(?=.*0:i32)(?=.*12:i32).*\[4-8\]:raw
; OFF: OFFSET-COLLAPSED

; ModuleID = 'tests/c/idsa_same_node.c'
source_filename = "tests/c/idsa_same_node.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.s = type { i32, i32, i32, i32 }

@__const.main.x = private unnamed_addr constant %struct.s { i32 0, i32 1, i32 2, i32 3 }, align 4

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca %struct.s, align 4
  %3 = alloca ptr, align 8
  store i32 0, ptr %1, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %2, ptr align 4 @__const.main.x, i64 16, i1 false)
  %4 = call i32 @nd_int()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %6, label %8

6:                                                ; preds = %0
  %7 = getelementptr inbounds %struct.s, ptr %2, i32 0, i32 1
  br label %10

8:                                                ; preds = %0
  %9 = getelementptr inbounds %struct.s, ptr %2, i32 0, i32 2
  br label %10

10:                                               ; preds = %8, %6
  %11 = phi ptr [ %7, %6 ], [ %9, %8 ]
  store ptr %11, ptr %3, align 8
  %12 = load ptr, ptr %3, align 8
  store i32 5, ptr %12, align 4
  %13 = getelementptr inbounds %struct.s, ptr %2, i32 0, i32 0
  %14 = load i32, ptr %13, align 4
  %15 = getelementptr inbounds %struct.s, ptr %2, i32 0, i32 3
  %16 = load i32, ptr %15, align 4
  %17 = add nsw i32 %14, %16
  ret i32 %17
}

; Function Attrs: nocallback nofree nounwind willreturn memory(argmem: readwrite)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

declare i32 @nd_int() #2

attributes #0 = { noinline nounwind uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }
attributes #2 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 18.1.8 (++20240731024944+3b5b5c1ec4a3-1~exp1~20240731145000.144)"}
