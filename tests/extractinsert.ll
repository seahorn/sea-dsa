; RUN: %seadsa %s %cs_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-outdir=%T/extractinsert.ll
; RUN: %cmp-graphs %tests/extractinsert.entry.mem.dot %T/extractinsert.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'param.c.ll'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define void @entry(ptr %a, ptr %b) #0 {
bb:
  %agg1ptr = bitcast ptr %a to ptr
  %agg2ptr = bitcast ptr %b to ptr
  %agg1 = load { ptr, i32 }, ptr %agg1ptr, align 8
  %agg2 = load { float, ptr }, ptr %agg2ptr, align 8
  %ch = alloca i8, align 1
  %ins_ch = insertvalue { ptr, i32 } %agg1, ptr %ch, 0
  %ext_ch = extractvalue { ptr, i32 } %agg1, 0
  %ins_ch2 = insertvalue { float, ptr } %agg2, ptr %ext_ch, 1
  ret void
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.8.1 (tags/RELEASE_381/final)"}
