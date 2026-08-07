; RUN: %seadsa %s %cs_dsa --sea-dsa-dot -sea-dsa-stats --sea-dsa-type-aware=true --sea-dsa-dot-outdir=%T/arrays.local.ll
; RUN: %cmp-graphs %tests/arrays.local.entry.mem.dot %T/arrays.local.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'arrays.c.ll'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Foo = type { i32, ptr }

; Function Attrs: nounwind uwtable
define void @entry(i32 %n) #0 {
bb:
  %tmp = alloca i32, align 4
  %a = alloca i32, align 4
  %ints = alloca ptr, align 8
  %i = alloca i32, align 4
  %f = alloca float, align 4
  %foos = alloca ptr, align 8
  %i1 = alloca i32, align 4
  store i32 %n, ptr %tmp, align 4
  store i32 1, ptr %a, align 4
  %tmp1 = call ptr (...) @get_int_arr()
  store ptr %tmp1, ptr %ints, align 8
  store i32 0, ptr %i, align 4
  br label %bb2

bb2:                                              ; preds = %bb11, %bb
  %tmp3 = load i32, ptr %i, align 4
  %tmp4 = load i32, ptr %tmp, align 4
  %tmp5 = icmp slt i32 %tmp3, %tmp4
  br i1 %tmp5, label %bb6, label %bb14

bb6:                                              ; preds = %bb2
  %tmp7 = load i32, ptr %i, align 4
  %tmp8 = sext i32 %tmp7 to i64
  %tmp9 = load ptr, ptr %ints, align 8
  %tmp10 = getelementptr inbounds ptr, ptr %tmp9, i64 %tmp8
  store ptr %a, ptr %tmp10, align 8
  br label %bb11

bb11:                                             ; preds = %bb6
  %tmp12 = load i32, ptr %i, align 4
  %tmp13 = add nsw i32 %tmp12, 1
  store i32 %tmp13, ptr %i, align 4
  br label %bb2

bb14:                                             ; preds = %bb2
  %tmp15 = call ptr (...) @get_foo_arr()
  store ptr %tmp15, ptr %foos, align 8
  store i32 0, ptr %i1, align 4
  br label %bb16

bb16:                                             ; preds = %bb27, %bb14
  %tmp17 = load i32, ptr %i1, align 4
  %tmp18 = load i32, ptr %tmp, align 4
  %tmp19 = icmp slt i32 %tmp17, %tmp18
  br i1 %tmp19, label %bb20, label %bb30

bb20:                                             ; preds = %bb16
  %tmp21 = load i32, ptr %i1, align 4
  %tmp22 = mul nsw i32 2, %tmp21
  %tmp23 = sext i32 %tmp22 to i64
  %tmp24 = load ptr, ptr %foos, align 8
  %tmp25 = getelementptr inbounds %struct.Foo, ptr %tmp24, i64 %tmp23
  %tmp26 = getelementptr inbounds %struct.Foo, ptr %tmp25, i32 0, i32 1
  store ptr %f, ptr %tmp26, align 8
  br label %bb27

bb27:                                             ; preds = %bb20
  %tmp28 = load i32, ptr %i1, align 4
  %tmp29 = add nsw i32 %tmp28, 1
  store i32 %tmp29, ptr %i1, align 4
  br label %bb16

bb30:                                             ; preds = %bb16
  ret void
}

declare ptr @get_int_arr(...) #1

declare ptr @get_foo_arr(...) #1

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.8.1 (tags/RELEASE_381/final)"}
