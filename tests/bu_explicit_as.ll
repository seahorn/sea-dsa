; RUN: %seadsa %s %butd_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-print-as --sea-dsa-dot-outdir=%T/bu_explicit_as.ll
; RUN: %cmp-graphs %tests/bu_explicit_as.main.mem.dot %T/bu_explicit_as.ll/main.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$
; XFAIL: *

; ModuleID = 'bu_explicit_as.ll'
source_filename = "bu_explicit_as.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@str1 = internal constant [5 x i8] c"str1\00", align 1
@str2 = internal constant [5 x i8] c"str2\00", align 1

; Function Attrs: nounwind uwtable
define ptr @foo(i32 %0) #0 {
  %2 = icmp ne i32 %0, 0
  %3 = zext i1 %2 to i64
  %4 = select i1 %2, ptr @str1, ptr @str2
  call void @print(ptr %4)
  ret ptr @str1
}

declare void @print(ptr) #1

; Function Attrs: nounwind uwtable
define i32 @main(i32 %0, ptr %1) #0 {
  %3 = call ptr @foo(i32 %0)
  call void @print(ptr %3)
  ret i32 0
}

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 5.0.1-4 (tags/RELEASE_501/final)"}
