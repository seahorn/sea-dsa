; RUN: %seadsa %s %cs_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-outdir=%T/globals.ll
; RUN: %cmp-graphs %tests/globals.entry.mem.dot %T/globals.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$


@a = common global i32 0, align 4
@b = common global i32 0, align 4
@c = common global i32 0, align 4
@f = common global float 0.000000e+00, align 4
@g = common global float 0.000000e+00, align 4
@h = common global float 0.000000e+00, align 4
@v = common global ptr null, align 8

; Function Attrs: nounwind uwtable
define i32 @entry(i32 %arc, ptr %argv) #0 {
bb:
  %tmp1 = alloca i32, align 4
  %tmp2 = alloca ptr, align 8
  %aa = alloca ptr, align 8
  %fp = alloca ptr, align 8
  %gp = alloca ptr, align 8
  %tmp = alloca ptr, align 8
  %str = alloca ptr, align 8
  store i32 %arc, ptr %tmp1, align 4
  store ptr %argv, ptr %tmp2, align 8
  store i32 1, ptr @a, align 4
  %tmp3 = load i32, ptr @a, align 4
  %tmp4 = add nsw i32 %tmp3, 1
  store i32 %tmp4, ptr @b, align 4
  %tmp5 = load i32, ptr @b, align 4
  %tmp6 = add nsw i32 %tmp5, 1
  store i32 %tmp6, ptr @c, align 4
  store float 4.000000e+00, ptr @f, align 4
  %tmp7 = load float, ptr @f, align 4
  %tmp8 = fadd float %tmp7, 1.000000e+00
  store float %tmp8, ptr @g, align 4
  %tmp9 = load float, ptr @g, align 4
  %tmp10 = fadd float %tmp9, 1.000000e+00
  store float %tmp10, ptr @h, align 4
  store ptr @entry, ptr @v, align 8
  store ptr @a, ptr %aa, align 8
  store ptr @f, ptr %fp, align 8
  store ptr @g, ptr %gp, align 8
  store ptr null, ptr %tmp, align 8
  %tmp11 = load i32, ptr @a, align 4
  %tmp12 = icmp ne i32 %tmp11, 0
  br i1 %tmp12, label %bb13, label %bb14

bb13:                                             ; preds = %bb
  store ptr %fp, ptr %tmp, align 8
  br label %bb15

bb14:                                             ; preds = %bb
  store ptr %gp, ptr %tmp, align 8
  br label %bb15

bb15:                                             ; preds = %bb14, %bb13
  %tmp16 = load ptr, ptr %tmp2, align 8
  %tmp17 = getelementptr inbounds ptr, ptr %tmp16, i64 0
  %tmp18 = load ptr, ptr %tmp17, align 8
  store ptr %tmp18, ptr %str, align 8
  ret i32 0
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.8.1 (tags/RELEASE_381/final)"}
