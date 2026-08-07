; RUN: %seadsa  %flat_dsa --sea-dsa-dot %s --sea-dsa-stats --sea-dsa-dot-outdir=%T/test-2.flat.ll
; RUN: %cmp-graphs %tests/test-2.flat.c.main.mem.dot %T/test-2.flat.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-2.ci.pp.ms.o.bc'
target datalayout = "e-m:o-p:32:32-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.11.0"

%struct.node = type { ptr, ptr }
%struct.element = type { i32, i32 }

@llvm.used = appending global [8 x ptr] [ptr @seahorn.fail, ptr @verifier.assume, ptr @verifier.assume.not, ptr @verifier.error, ptr @verifier.assume, ptr @verifier.assume.not, ptr @verifier.error, ptr @seahorn.fail], section "llvm.metadata"

; Function Attrs: nounwind ssp
define internal fastcc ptr @mkList(i32 %sz, ptr %e) unnamed_addr #0 {
  call void @seahorn.fn.enter() #3
  %1 = icmp slt i32 %sz, 1
  br i1 %1, label %17, label %2

2:                                                ; preds = %0
  %3 = call ptr @malloc(i32 8) #3
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
  %14 = call ptr @malloc(i32 8) #3
  %15 = bitcast ptr %p.0 to ptr
  store ptr %14, ptr %15, align 4
  %.cast = bitcast ptr %14 to ptr
  %16 = add nsw i32 %i.0, 1
  br label %5

17:                                               ; preds = %11, %5, %0
  %.0 = phi ptr [ null, %0 ], [ %4, %11 ], [ %4, %5 ]
  ret ptr %.0
}

declare ptr @malloc(i32) #1

; Function Attrs: nounwind ssp
define i32 @main() #0 {
  call void @seahorn.fn.enter() #3
  %malloc1 = alloca %struct.element, align 4
  %1 = getelementptr inbounds %struct.element, ptr %malloc1, i32 0, i32 0
  store i32 5, ptr %1, align 4
  %2 = getelementptr inbounds %struct.element, ptr %malloc1, i32 0, i32 1
  store i32 6, ptr %2, align 4
  %3 = call fastcc ptr @mkList(i32 5, ptr nonnull %malloc1)
  %4 = call fastcc ptr @mkList(i32 5, ptr nonnull %malloc1)
  br label %5

5:                                                ; preds = %7, %0
  %p1.0 = phi ptr [ %3, %0 ], [ %13, %7 ]
  %6 = icmp eq ptr %p1.0, null
  br i1 %6, label %14, label %7

7:                                                ; preds = %5
  %8 = getelementptr inbounds %struct.node, ptr %p1.0, i32 0, i32 1
  %9 = load ptr, ptr %8, align 4
  %10 = getelementptr inbounds %struct.element, ptr %9, i32 0, i32 0
  %11 = load i32, ptr %10, align 4
  call void @print(i32 %11) #3
  %12 = getelementptr inbounds %struct.node, ptr %p1.0, i32 0, i32 0
  %13 = load ptr, ptr %12, align 4
  br label %5

14:                                               ; preds = %16, %5
  %p2.0 = phi ptr [ %22, %16 ], [ %4, %5 ]
  %15 = icmp eq ptr %p2.0, null
  br i1 %15, label %23, label %16

16:                                               ; preds = %14
  %17 = getelementptr inbounds %struct.node, ptr %p2.0, i32 0, i32 1
  %18 = load ptr, ptr %17, align 4
  %19 = getelementptr inbounds %struct.element, ptr %18, i32 0, i32 1
  %20 = load i32, ptr %19, align 4
  call void @print(i32 %20) #3
  %21 = getelementptr inbounds %struct.node, ptr %p2.0, i32 0, i32 0
  %22 = load ptr, ptr %21, align 4
  br label %14

23:                                               ; preds = %14
  ret i32 0
}

declare void @print(i32) #1

declare void @verifier.assume(i1)

declare void @verifier.assume.not(i1)

declare void @seahorn.fail()

; Function Attrs: noreturn
declare void @verifier.error() #2

declare void @seahorn.fn.enter()

declare void @verifier.assert(i1)

attributes #0 = { nounwind ssp "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="yonah" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="yonah" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { noreturn }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 7, !"PIC Level", i32 2}
!1 = !{!"clang version 3.8.0 (tags/RELEASE_380/final)"}
