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
  %2 = alloca i32*, align 8
  %3 = alloca i32*, align 8
  %4 = alloca i32*, align 8
  %5 = alloca i32*, align 8
  store i32 0, i32* %1, align 4
  %6 = load i32*, i32** %2, align 8
  %7 = bitcast i32* %6 to i8*
  call void @sea_dsa_set_ptrtoint(i8* %7)
  %8 = load i32*, i32** %2, align 8
  %9 = bitcast i32* %8 to i8*
  call void @sea_dsa_set_modified(i8* %9)
  %10 = load i32*, i32** %2, align 8
  %11 = bitcast i32* %10 to i8*
  call void @sea_dsa_set_read(i8* %11)
  %12 = load i32*, i32** %3, align 8
  %13 = bitcast i32* %12 to i8*
  call void @sea_dsa_set_ptrtoint(i8* %13)
  %14 = load i32*, i32** %4, align 8
  %15 = bitcast i32* %14 to i8*
  call void @sea_dsa_set_modified(i8* %15)
  %16 = load i32*, i32** %5, align 8
  %17 = bitcast i32* %16 to i8*
  call void @sea_dsa_set_read(i8* %17)
  ret i32 0
}

declare dso_local void @sea_dsa_set_ptrtoint(i8*) #1

declare dso_local void @sea_dsa_set_modified(i8*) #1

declare dso_local void @sea_dsa_set_read(i8*) #1

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.1-++20200519100828+f79cd71e145-1~exp1~20200519201452.38 "}
