; RUN: %seadsa  %cs_dsa --sea-dsa-dot %s --sea-dsa-stats --sea-dsa-dot-outdir=%T/test-1.cs.ll
; RUN: %cmp-graphs %tests/test-1.cs.c.main.mem.dot %T/test-1.cs.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-1.ci.pp.ms.o.bc'
target datalayout = "e-m:o-p:32:32-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.11.0"

@llvm.used = appending global [8 x ptr] [ptr @seahorn.fail, ptr @verifier.assume, ptr @verifier.assume.not, ptr @verifier.error, ptr @verifier.assume, ptr @verifier.assume.not, ptr @verifier.error, ptr @seahorn.fail], section "llvm.metadata"

; Function Attrs: nounwind ssp
define internal fastcc void @f(ptr %x, ptr %y) unnamed_addr #0 {
  call void @seahorn.fn.enter() #3
  store i32 1, ptr %x, align 4
  store i32 2, ptr %y, align 4
  ret void
}

; Function Attrs: nounwind ssp
define internal fastcc void @g(ptr %p, ptr %q, ptr %r, ptr %s) unnamed_addr #0 {
  call void @seahorn.fn.enter() #3
  call fastcc void @f(ptr %p, ptr %q)
  call fastcc void @f(ptr %r, ptr %s)
  ret void
}

; Function Attrs: nounwind ssp
define i32 @main(i32 %argc, ptr %argv) #0 {
  call void @seahorn.fn.enter() #3
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  %w = alloca i32, align 4
  %z = alloca i32, align 4
  %1 = call i32 @nd() #3
  %2 = icmp eq i32 %1, 0
  %x.y = select i1 %2, ptr %x, ptr %y
  call fastcc void @g(ptr %x.y, ptr nonnull %y, ptr nonnull %w, ptr nonnull %z)
  %3 = load i32, ptr %x, align 4
  %4 = load i32, ptr %y, align 4
  %5 = add nsw i32 %3, %4
  %6 = load i32, ptr %w, align 4
  %7 = add nsw i32 %5, %6
  %8 = load i32, ptr %z, align 4
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

!0 = !{i32 7, !"PIC Level", i32 2}
!1 = !{!"clang version 3.8.0 (tags/RELEASE_380/final)"}
