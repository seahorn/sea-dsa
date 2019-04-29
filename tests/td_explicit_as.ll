; RUN: %seadsa %s %butd_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-print-as --sea-dsa-dot-outdir=%T/td_explicit_as.ll
; RUN: %cmp-graphs %tests/td_explicit_as.entry.mem.dot %T/td_explicit_as.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$


; ModuleID = 'td_explicit_as.c'
source_filename = "td_explicit_as.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str1 = internal constant [5 x i8] c"str1\00", align 1
@.str = private unnamed_addr constant [5 x i8] c"str2\00", align 1

; Function Attrs: nounwind uwtable
define void @foo(i8*) local_unnamed_addr #0 {
  tail call void @print(i8* %0) #2
  ret void
}

declare void @print(i8*) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define void @entry(i8*) local_unnamed_addr #0 {
  tail call void @print(i8* %0) #2
  ret void
}

; Function Attrs: nounwind uwtable
define i32 @main(i32, i8** nocapture readnone) local_unnamed_addr #0 {
  %3 = icmp sgt i32 %0, 2
  %4 = select i1 %3, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @str1, i64 0, i64 0), i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str, i64 0, i64 0)
  tail call void @foo(i8* %4)
  tail call void @entry(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @str1, i64 0, i64 0))
  ret i32 0
}

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 5.0.2 (tags/RELEASE_502/final)"}
