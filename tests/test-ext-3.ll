; RUN: %seadsa  %cs_dsa --sea-dsa-dot %s --sea-dsa-stats --sea-dsa-dot-outdir=%T/test-ext-3.ll
; RUN: %cmp-graphs %tests/test-ext-3.c.main.mem.dot %T/test-ext-3.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-ext-3.c'
source_filename = "test-ext-3.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  %4 = alloca ptr, align 8
  %5 = alloca ptr, align 8
  %6 = alloca ptr, align 8
  %7 = alloca ptr, align 8
  %8 = alloca ptr, align 8
  store i32 0, ptr %1, align 4
  %9 = load ptr, ptr %2, align 8
  %10 = bitcast ptr %9 to ptr
  call void @sea_dsa_set_ptrtoint(ptr %10)
  %11 = load ptr, ptr %2, align 8
  %12 = bitcast ptr %11 to ptr
  call void @sea_dsa_set_inttoptr(ptr %12)
  %13 = load ptr, ptr %2, align 8
  %14 = bitcast ptr %13 to ptr
  call void @sea_dsa_set_modified(ptr %14)
  %15 = load ptr, ptr %2, align 8
  %16 = bitcast ptr %15 to ptr
  call void @sea_dsa_set_read(ptr %16)
  %17 = load ptr, ptr %2, align 8
  %18 = bitcast ptr %17 to ptr
  call void @sea_dsa_set_heap(ptr %18)
  %19 = load ptr, ptr %2, align 8
  %20 = bitcast ptr %19 to ptr
  call void @sea_dsa_set_alloca(ptr %20)
  %21 = load ptr, ptr %3, align 8
  %22 = bitcast ptr %21 to ptr
  call void @sea_dsa_set_ptrtoint(ptr %22)
  %23 = load ptr, ptr %4, align 8
  %24 = bitcast ptr %23 to ptr
  call void @sea_dsa_set_inttoptr(ptr %24)
  %25 = load ptr, ptr %5, align 8
  %26 = bitcast ptr %25 to ptr
  call void @sea_dsa_set_modified(ptr %26)
  %27 = load ptr, ptr %6, align 8
  %28 = bitcast ptr %27 to ptr
  call void @sea_dsa_set_read(ptr %28)
  %29 = load ptr, ptr %7, align 8
  %30 = bitcast ptr %29 to ptr
  call void @sea_dsa_set_heap(ptr %30)
  %31 = load ptr, ptr %8, align 8
  %32 = bitcast ptr %31 to ptr
  call void @sea_dsa_set_alloca(ptr %32)
  ret i32 0
}

declare dso_local void @sea_dsa_set_ptrtoint(ptr) #1

declare dso_local void @sea_dsa_set_inttoptr(ptr) #1

declare dso_local void @sea_dsa_set_modified(ptr) #1

declare dso_local void @sea_dsa_set_read(ptr) #1

declare dso_local void @sea_dsa_set_heap(ptr) #1

declare dso_local void @sea_dsa_set_alloca(ptr) #1

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.1-++20200519100828+f79cd71e145-1~exp1~20200519201452.38 "}
