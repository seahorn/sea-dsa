; RUN: %seadsa %s %cs_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-outdir=%T/structs.local.ll
; RUN: %cmp-graphs %tests/structs.local.entry.mem.dot %T/structs.local.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'structs.local.c.ll'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.A = type { i32, float }
%struct.B = type { ptr, ptr }

@entry.a = private unnamed_addr constant %struct.A { i32 1, float 0x40090A3D80000000 }, align 4

; Function Attrs: nounwind uwtable
define void @entry() #0 {
bb:
  %a = alloca %struct.A, align 4
  %f = alloca float, align 4
  %b = alloca %struct.B, align 8
  %ii = alloca ptr, align 8
  %tmp = bitcast ptr %a to ptr
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %tmp, ptr align 4 @entry.a, i64 8, i1 false)
  store float 4.200000e+01, ptr %f, align 4
  %tmp1 = getelementptr inbounds %struct.B, ptr %b, i32 0, i32 0
  store ptr %f, ptr %tmp1, align 8
  %tmp2 = getelementptr inbounds %struct.B, ptr %b, i32 0, i32 1
  store ptr %a, ptr %tmp2, align 8
  %tmp3 = getelementptr inbounds %struct.B, ptr %b, i32 0, i32 1
  %tmp4 = load ptr, ptr %tmp3, align 8
  %tmp5 = getelementptr inbounds %struct.A, ptr %tmp4, i32 0, i32 0
  store ptr %tmp5, ptr %ii, align 8
  ret void
}

; Function Attrs: argmemonly nocallback nofree nounwind willreturn
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nocallback nofree nounwind willreturn }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.8.1 (tags/RELEASE_381/final)"}
