; RUN: %seadsa  %cs_dsa --sea-dsa-dot %s --sea-dsa-stats --sea-dsa-dot-outdir=%T/test-ext-1.ll
; RUN: %cmp-graphs %tests/test-ext-1.c.main.mem.dot %T/test-ext-1.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-ext-1.bc'
target datalayout = "e-m:o-p:32:32-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.11.0"

%struct.node = type { ptr, ptr }
%struct.element = type { i32, i32 }

@llvm.used = appending global [4 x ptr] [ptr @seahorn.fail, ptr @verifier.assume, ptr @verifier.assume.not, ptr @verifier.error], section "llvm.metadata"

; Function Attrs: nounwind ssp
define internal fastcc ptr @mkList(i32 %sz, ptr %e) unnamed_addr #0 {
  call void @seahorn.fn.enter() #3
  %1 = icmp slt i32 %sz, 1
  br i1 %1, label %17, label %2

2:                                                ; preds = %0
  %3 = call ptr @mymalloc(i32 8) #3
  %4 = bitcast ptr %3 to ptr
  br label %5

5:                                                ; preds = %13, %2
  %p.0 = phi ptr [ %4, %2 ], [ %.cast, %13 ]
  %i.0 = phi i32 [ 0, %2 ], [ %16, %13 ]
  %6 = icmp slt i32 %i.0, %sz
  br i1 %6, label %7, label %17

7:                                                ; preds = %5
  %8 = getelementptr inbounds %struct.node, ptr %p.0, i32 0, i32 1
  store ptr %e, ptr %8, align 4
  %9 = add nsw i32 %sz, -1
  %10 = icmp eq i32 %i.0, %9
  br i1 %10, label %11, label %13

11:                                               ; preds = %7
  %12 = getelementptr inbounds %struct.node, ptr %p.0, i32 0, i32 0
  store ptr null, ptr %12, align 4
  br label %17

13:                                               ; preds = %7
  %14 = call ptr @mymalloc(i32 8) #3
  %15 = bitcast ptr %p.0 to ptr
  store ptr %14, ptr %15, align 4
  %.cast = bitcast ptr %14 to ptr
  %16 = add nsw i32 %i.0, 1
  br label %5

17:                                               ; preds = %11, %5, %0
  %.0 = phi ptr [ null, %0 ], [ %4, %11 ], [ %4, %5 ]
  ret ptr %.0
}

declare ptr @mymalloc(i32) #1

; Function Attrs: nounwind ssp
define i32 @main() #0 {
  call void @seahorn.fn.enter() #3
  %1 = call ptr @mymalloc(i32 8) #3
  %2 = bitcast ptr %1 to ptr
  %3 = bitcast ptr %1 to ptr
  store i32 5, ptr %3, align 4
  %4 = getelementptr inbounds i8, ptr %1, i32 4
  %5 = bitcast ptr %4 to ptr
  store i32 6, ptr %5, align 4
  %6 = call fastcc ptr @mkList(i32 5, ptr %2)
  %7 = call fastcc ptr @mkList(i32 5, ptr %2)
  br label %8

8:                                                ; preds = %10, %0
  %p1.0 = phi ptr [ %6, %0 ], [ %16, %10 ]
  %9 = icmp eq ptr %p1.0, null
  br i1 %9, label %17, label %10

10:                                               ; preds = %8
  %11 = getelementptr inbounds %struct.node, ptr %p1.0, i32 0, i32 1
  %12 = load ptr, ptr %11, align 4
  %13 = getelementptr inbounds %struct.element, ptr %12, i32 0, i32 0
  %14 = load i32, ptr %13, align 4
  call void @print(i32 %14) #3
  %15 = getelementptr inbounds %struct.node, ptr %p1.0, i32 0, i32 0
  %16 = load ptr, ptr %15, align 4
  br label %8

17:                                               ; preds = %19, %8
  %p2.0 = phi ptr [ %25, %19 ], [ %7, %8 ]
  %18 = icmp eq ptr %p2.0, null
  br i1 %18, label %26, label %19

19:                                               ; preds = %17
  %20 = getelementptr inbounds %struct.node, ptr %p2.0, i32 0, i32 1
  %21 = load ptr, ptr %20, align 4
  %22 = getelementptr inbounds %struct.element, ptr %21, i32 0, i32 1
  %23 = load i32, ptr %22, align 4
  call void @print(i32 %23) #3
  %24 = getelementptr inbounds %struct.node, ptr %p2.0, i32 0, i32 0
  %25 = load ptr, ptr %24, align 4
  br label %17

26:                                               ; preds = %17
  %27 = bitcast ptr %p1.0 to ptr
  call void (ptr, ...) @sea_dsa_alias(ptr %27, ptr %p2.0) #3
  ret i32 0
}

declare void @print(i32) #1

declare void @sea_dsa_alias(ptr, ...) #1

declare void @verifier.assume(i1)

declare void @verifier.assume.not(i1)

declare void @seahorn.fail()

; Function Attrs: noreturn
declare void @verifier.error() #2

declare void @seahorn.fn.enter()

attributes #0 = { nounwind ssp "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="yonah" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="yonah" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { noreturn }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 7, !"PIC Level", i32 2}
!1 = !{!"clang version 3.8.0 (tags/RELEASE_380/final)"}
