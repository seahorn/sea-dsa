; RUN: %seadsa -sea-dsa-shadow-mem %cs_dsa %s -oll %s.inst.ll --outdir=output
; OutputCheck --file-to-check=output/%s.inst.ll %s -d --comment=";"

; CHECK-L: %sm3 = call i32 @shadow.mem.init(i32 0, i8* null), !shadow.mem !3, !shadow.mem.def !3
; CHECK-L: %sm = call i32 @shadow.mem.store(i32 0, i32 %sm3, i8* null), !shadow.mem !3, !shadow.mem.def !4
; CHECK-L: %sm1 = call i32 @shadow.mem.store(i32 0, i32 %sm, i8* null), !shadow.mem !3, !shadow.mem.def !4
; CHECK-L: %sm2 = call i32 @shadow.mem.store(i32 0, i32 %sm1, i8* null), !shadow.mem !3, !shadow.mem.def !4
; CHECK-L: call void @shadow.mem.load(i32 0, i32 %sm, i8* null), !shadow.mem !3, !shadow.mem.use !4
; CHECK-L: call void @shadow.mem.load(i32 0, i32 %sm1, i8* null), !shadow.mem !3, !shadow.mem.use !4

; ModuleID = 'shadow_mem_2.c'
source_filename = "shadow_mem_2.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

; Function Attrs: nounwind ssp uwtable
define i32 @main() local_unnamed_addr #0 {
  %1 = tail call i32* @freshInt() #3
  %2 = tail call i32* @freshInt() #3
  tail call void @sea_dsa_alias(i32* %1, i32* %2) #3
  store i32 33, i32* %1, align 4, !tbaa !3
  %3 = getelementptr inbounds i32, i32* %1, i64 1
  store i32 44, i32* %3, align 4, !tbaa !3
  store i32 52, i32* %2, align 4, !tbaa !3
  %4 = load i32, i32* %1, align 4, !tbaa !3
  %5 = load i32, i32* %3, align 4, !tbaa !3
  %6 = add nsw i32 %5, %4
  %7 = icmp eq i32 %6, 96
  br i1 %7, label %9, label %8

; <label>:8:                                      ; preds = %0
  tail call void @__VERIFIER_error() #4
  unreachable

; <label>:9:                                      ; preds = %0
  ret i32 0
}

declare i32* @freshInt() local_unnamed_addr #1

declare void @sea_dsa_alias(i32*, i32*) local_unnamed_addr #1

; Function Attrs: noreturn
declare void @__VERIFIER_error() local_unnamed_addr #2

attributes #0 = { nounwind ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { noreturn "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }
attributes #4 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
!3 = !{!4, !4, i64 0}
!4 = !{!"int", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C/C++ TBAA"}
