; RUN: %seadsa %s %butd_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-print-as --sea-dsa-dot-outdir=%T/bu_affected_globals.ll
; RUN: %cmp-graphs %tests/bu_affected_globals.entry.mem.dot %T/bu_affected_globals.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'bu_affected_globals.ll'
source_filename = "bu_affected_globals.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@storage = internal global i8* null, align 8
@confuse = internal global i8* null, align 8

; Function Attrs: nounwind uwtable
define void @foo() #0 {
bb:
  %tmp = call i8* @malloc(i64 64)
  store i8* %tmp, i8** @storage, align 8, !tbaa !2
  %tmp1 = load i8*, i8** @storage, align 8, !tbaa !2
  %tmp2 = icmp ne i8* %tmp1, null
  %confuse.sink = select i1 %tmp2, i8** @storage, i8** @confuse
  %tmp3 = load i8*, i8** %confuse.sink, align 8, !tbaa !2
  call void @print(i8* %tmp3)
  ret void
}

declare i8* @malloc(i64) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64, i8* nocapture) #2

declare void @print(i8*) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64, i8* nocapture) #2

; Function Attrs: nounwind uwtable
define void @entry() #0 {
bb:
  call void @foo()
  %tmp = load i8*, i8** @storage, align 8, !tbaa !2
  call void @print(i8* %tmp)
  ret void
}

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 5.0.1-4 (tags/RELEASE_501/final)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"any pointer", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
