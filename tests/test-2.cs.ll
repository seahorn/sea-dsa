; RUN: %seadsa  %cs_dsa --sea-dsa-dot %s --sea-dsa-stats --sea-dsa-dot-outdir=%T/test-2.cs.ll
; RUN: %cmp-graphs %tests/test-2.cs.c.main.mem.dot %T/test-2.cs.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-2.ci.pp.ms.o.bc'
target datalayout = "e-m:o-p:32:32-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.11.0"

%struct.node = type { %struct.node*, %struct.element* }
%struct.element = type { i32, i32 }

@llvm.used = appending global [8 x i8*] [i8* bitcast (void ()* @seahorn.fail to i8*), i8* bitcast (void (i1)* @verifier.assume to i8*), i8* bitcast (void (i1)* @verifier.assume.not to i8*), i8* bitcast (void ()* @verifier.error to i8*), i8* bitcast (void (i1)* @verifier.assume to i8*), i8* bitcast (void (i1)* @verifier.assume.not to i8*), i8* bitcast (void ()* @verifier.error to i8*), i8* bitcast (void ()* @seahorn.fail to i8*)], section "llvm.metadata"

; Function Attrs: nounwind ssp
define internal fastcc %struct.node* @mkList(i32 %sz, %struct.element* %e) unnamed_addr #0 {
  call void @seahorn.fn.enter() #3
  %1 = icmp slt i32 %sz, 1
  br i1 %1, label %17, label %2

; <label>:2                                       ; preds = %0
  %3 = call i8* @malloc(i32 8) #3
  %4 = bitcast i8* %3 to %struct.node*
  br label %5

; <label>:5                                       ; preds = %13, %2
  %p.0 = phi %struct.node* [ %4, %2 ], [ %.cast, %13 ]
  %i.0 = phi i32 [ 0, %2 ], [ %16, %13 ]
  %6 = icmp slt i32 %i.0, %sz
  br i1 %6, label %7, label %17

; <label>:7                                       ; preds = %5
  %8 = getelementptr inbounds %struct.node, %struct.node* %p.0, i32 0, i32 1
  store %struct.element* %e, %struct.element** %8, align 4
  %9 = add nsw i32 %sz, -1
  %10 = icmp eq i32 %i.0, %9
  br i1 %10, label %11, label %13

; <label>:11                                      ; preds = %7
  %12 = getelementptr inbounds %struct.node, %struct.node* %p.0, i32 0, i32 0
  store %struct.node* null, %struct.node** %12, align 4
  br label %17

; <label>:13                                      ; preds = %7
  %14 = call i8* @malloc(i32 8) #3
  %15 = bitcast %struct.node* %p.0 to i8**
  store i8* %14, i8** %15, align 4
  %.cast = bitcast i8* %14 to %struct.node*
  %16 = add nsw i32 %i.0, 1
  br label %5

; <label>:17                                      ; preds = %11, %5, %0
  %.0 = phi %struct.node* [ null, %0 ], [ %4, %11 ], [ %4, %5 ]
  ret %struct.node* %.0
}

declare i8* @malloc(i32) #1

; Function Attrs: nounwind ssp
define i32 @main() #0 {
  call void @seahorn.fn.enter() #3
  %malloc1 = alloca %struct.element, align 4
  %1 = getelementptr inbounds %struct.element, %struct.element* %malloc1, i32 0, i32 0
  store i32 5, i32* %1, align 4
  %2 = getelementptr inbounds %struct.element, %struct.element* %malloc1, i32 0, i32 1
  store i32 6, i32* %2, align 4
  %3 = call fastcc %struct.node* @mkList(i32 5, %struct.element* nonnull %malloc1)
  %4 = call fastcc %struct.node* @mkList(i32 5, %struct.element* nonnull %malloc1)
  br label %5

; <label>:5                                       ; preds = %7, %0
  %p1.0 = phi %struct.node* [ %3, %0 ], [ %13, %7 ]
  %6 = icmp eq %struct.node* %p1.0, null
  br i1 %6, label %14, label %7

; <label>:7                                       ; preds = %5
  %8 = getelementptr inbounds %struct.node, %struct.node* %p1.0, i32 0, i32 1
  %9 = load %struct.element*, %struct.element** %8, align 4
  %10 = getelementptr inbounds %struct.element, %struct.element* %9, i32 0, i32 0
  %11 = load i32, i32* %10, align 4
  call void @print(i32 %11) #3
  %12 = getelementptr inbounds %struct.node, %struct.node* %p1.0, i32 0, i32 0
  %13 = load %struct.node*, %struct.node** %12, align 4
  br label %5

; <label>:14                                      ; preds = %16, %5
  %p2.0 = phi %struct.node* [ %22, %16 ], [ %4, %5 ]
  %15 = icmp eq %struct.node* %p2.0, null
  br i1 %15, label %23, label %16

; <label>:16                                      ; preds = %14
  %17 = getelementptr inbounds %struct.node, %struct.node* %p2.0, i32 0, i32 1
  %18 = load %struct.element*, %struct.element** %17, align 4
  %19 = getelementptr inbounds %struct.element, %struct.element* %18, i32 0, i32 1
  %20 = load i32, i32* %19, align 4
  call void @print(i32 %20) #3
  %21 = getelementptr inbounds %struct.node, %struct.node* %p2.0, i32 0, i32 0
  %22 = load %struct.node*, %struct.node** %21, align 4
  br label %14

; <label>:23                                      ; preds = %14
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

!0 = !{i32 1, !"PIC Level", i32 2}
!1 = !{!"clang version 3.8.0 (tags/RELEASE_380/final)"}
