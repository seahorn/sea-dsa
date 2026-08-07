; RUN: %seadsa %s %cs_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-outdir=%T/params.ll
; RUN: %cmp-graphs %tests/params.entry.mem.dot %T/params.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'param.c.ll'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define void @entry(ptr %argv) #0 {
bb:
  %tmp = alloca ptr, align 8
  %str = alloca ptr, align 8
  store ptr %argv, ptr %tmp, align 8
  %tmp1 = load ptr, ptr %tmp, align 8
  %tmp2 = getelementptr inbounds ptr, ptr %tmp1, i64 0
  %tmp3 = load ptr, ptr %tmp2, align 8
  store ptr %tmp3, ptr %str, align 8
  ret void
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.8.1 (tags/RELEASE_381/final)"}
