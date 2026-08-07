; RUN: %seadsa %s --sea-dsa-dot --sea-dsa-type-aware=true --sea-dsa-dot-print-as --sea-dsa-dot-outdir=%T/atomic_cas_ptr.ll
; RUN: %cmp-graphs %tests/atomic_cas_ptr.ll.dot %T/atomic_cas_ptr.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'atomic_cas_ptr.ll'
source_filename = "./c/atomic_cas_ptr.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

; Function Attrs: noinline nounwind ssp uwtable
define i32 @main() #0 {
  %1 = alloca ptr, align 8
  %newVal = alloca ptr, align 8
  %2 = call ptr (...) @nd_ptr()
  %3 = call ptr (...) @nd_cmp()
  store ptr %3, ptr %1, align 8
  %4 = call ptr (...) @new_val()
  store ptr %4, ptr %newVal, align 8
  %5 = bitcast ptr %1 to ptr
  %6 = bitcast ptr %newVal to ptr
  %oldVal = load ptr, ptr %2, align 8
  %7 = cmpxchg weak ptr %2, ptr %5, ptr %6 monotonic monotonic, align 8
  %8 = extractvalue { ptr, i1 } %7, 0
  %9 = extractvalue { ptr, i1 } %7, 1
  br i1 %9, label %11, label %10

10:                                               ; preds = %0
  store ptr %8, ptr %1, align 8
  br label %11

11:                                               ; preds = %10, %0
  %12 = load ptr, ptr %2, align 8
  %13 = icmp eq ptr %12, %4
  call void @__VERIFIER_assert(i1 zeroext %13)
  ret i32 0
}

declare ptr @nd_ptr(...) #1

declare ptr @nd_cmp(...) #1

declare ptr @new_val(...) #1

declare void @__VERIFIER_assert(i1 zeroext) #1

attributes #0 = { noinline nounwind ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
