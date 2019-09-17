; RUN: %seadsa  %cs_dsa --sea-dsa-dot %s --sea-dsa-stats --sea-dsa-dot-outdir=%T/test-1.cs.ll
; RUN: %cmp-graphs %tests/test-1.cs.c.main.mem.dot %T/test-1.cs.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-1.ci.pp.ms.o.bc'
target datalayout = "e-m:o-p:32:32-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.11.0"

@llvm.used = appending global [8 x i8*] [i8* bitcast (void ()* @seahorn.fail to i8*), i8* bitcast (void (i1)* @verifier.assume to i8*), i8* bitcast (void (i1)* @verifier.assume.not to i8*), i8* bitcast (void ()* @verifier.error to i8*), i8* bitcast (void (i1)* @verifier.assume to i8*), i8* bitcast (void (i1)* @verifier.assume.not to i8*), i8* bitcast (void ()* @verifier.error to i8*), i8* bitcast (void ()* @seahorn.fail to i8*)], section "llvm.metadata"

; Function Attrs: nounwind ssp
define internal fastcc void @f(i32* %x, i32* %y) unnamed_addr #0 {
  call void @seahorn.fn.enter() #3
  store i32 1, i32* %x, align 4
  store i32 2, i32* %y, align 4
  ret void
}

; Function Attrs: nounwind ssp
define internal fastcc void @g(i32* %p, i32* %q, i32* %r, i32* %s) unnamed_addr #0 {
  call void @seahorn.fn.enter() #3
  call fastcc void @f(i32* %p, i32* %q)
  call fastcc void @f(i32* %r, i32* %s)
  ret void
}

; Function Attrs: nounwind ssp
define i32 @main(i32 %argc, i8** %argv) #0 {
  call void @seahorn.fn.enter() #3
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  %w = alloca i32, align 4
  %z = alloca i32, align 4
  %1 = call i32 bitcast (i32 (...)* @nd to i32 ()*)() #3
  %2 = icmp eq i32 %1, 0
  %x.y = select i1 %2, i32* %x, i32* %y
  call fastcc void @g(i32* %x.y, i32* nonnull %y, i32* nonnull %w, i32* nonnull %z)
  %3 = load i32, i32* %x, align 4
  %4 = load i32, i32* %y, align 4
  %5 = add nsw i32 %3, %4
  %6 = load i32, i32* %w, align 4
  %7 = add nsw i32 %5, %6
  %8 = load i32, i32* %z, align 4
  %9 = add nsw i32 %7, %8
  ret i32 %9
}

declare i32 @nd(...) #1

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
