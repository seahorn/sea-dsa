; RUN: %seadsa %s %butd_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-print-as --sea-dsa-dot-outdir=%T/bu_globals_loop.ll
; RUN: %cmp-graphs %tests/bu_globals_loop.entry.mem.dot %T/bu_globals_loop.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'bu_globals_loop.ll'
source_filename = "bu_globals_loop.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@A = internal global ptr null, align 8
@B = internal global ptr null, align 8
@C = internal global ptr null, align 8

; Function Attrs: nounwind uwtable
define void @foo(i32 %arg) #0 {
bb:
  %tmp = icmp ne i32 %arg, 0
  %tmp1 = zext i1 %tmp to i64
  %tmp2 = select i1 %tmp, ptr @A, ptr @B
  store ptr @C, ptr %tmp2, align 8, !tbaa !2
  store ptr @A, ptr @C, align 8, !tbaa !2
  ret void
}

; Function Attrs: nounwind uwtable
define void @entry(i32 %arg) #0 {
bb:
  call void @foo(i32 %arg)
  %tmp = load ptr, ptr @A, align 8, !tbaa !2
  call void @print(ptr %tmp)
  ret void
}

declare void @print(ptr) #1

; Function Attrs: argmemonly nocallback nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #2

; Function Attrs: argmemonly nocallback nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #2

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nocallback nofree nosync nounwind willreturn }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 5.0.1-4 (tags/RELEASE_501/final)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"any pointer", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
