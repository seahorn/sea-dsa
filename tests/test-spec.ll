; RUN: %seadsa  %butd_dsa --sea-dsa-dot %s --sea-dsa-dot-outdir=%T/test-spec.ll --sea-dsa-specfile=%S/spec_func.ll
; RUN: %cmp-graphs %tests/test-spec.c.main.mem.dot %T/test-spec.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; RUN: %seadsa  %cs_dsa --sea-dsa-dot %s --sea-dsa-dot-outdir=%T/test-spec.ll --sea-dsa-specfile=%S/spec_func.ll
; RUN: %cmp-graphs %tests/test-spec.c.main.mem.dot %T/test-spec.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; RUN: %seadsa  %ci_dsa --sea-dsa-dot %s --sea-dsa-dot-outdir=%T/test-spec.ll --sea-dsa-specfile=%S/spec_func.ll
; RUN: %cmp-graphs %tests/test-spec-ci.c.main.mem.dot %T/test-spec.ll/main.mem.dot | OutputCheck %s -d --comment=";"

; CHECK: ^OK$

; ModuleID = 'test-spec.c'
source_filename = "test-spec.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@__const.main.vowel = private unnamed_addr constant [6 x i8] c"aeiou\00", align 1
@.str = private unnamed_addr constant [23 x i8] c"success: %s \0Afail: %s\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local ptr @test_prop(ptr %0, i8 signext %1) #0 {
  %3 = alloca ptr, align 8
  %4 = alloca i8, align 1
  store ptr %0, ptr %3, align 8
  store i8 %1, ptr %4, align 1
  %5 = load ptr, ptr %3, align 8
  %6 = load i8, ptr %4, align 1
  %7 = sext i8 %6 to i32
  %8 = call ptr @spec_func(ptr %5, i32 %7)
  ret ptr %8
}

declare dso_local ptr @spec_func(ptr, i32) #1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca [6 x i8], align 1
  %3 = alloca i8, align 1
  %4 = alloca i8, align 1
  %5 = alloca ptr, align 8
  %6 = alloca ptr, align 8
  store i32 0, ptr %1, align 4
  %7 = bitcast ptr %2 to ptr
  call void @llvm.memcpy.p0.p0.i64(ptr align 1 %7, ptr align 1 @__const.main.vowel, i64 6, i1 false)
  store i8 105, ptr %3, align 1
  store i8 98, ptr %4, align 1
  %8 = getelementptr inbounds [6 x i8], ptr %2, i64 0, i64 0
  %9 = call ptr @test_prop(ptr %8, i8 signext 105)
  store ptr %9, ptr %5, align 8
  %10 = getelementptr inbounds [6 x i8], ptr %2, i64 0, i64 0
  %11 = call ptr @spec_func(ptr %10, i32 98)
  store ptr %11, ptr %6, align 8
  %12 = load ptr, ptr %5, align 8
  %13 = load ptr, ptr %6, align 8
  %14 = call i32 (ptr, ...) @printf(ptr @.str, ptr %12, ptr %13)
  ret i32 0
}

declare dso_local i32 @printf(ptr, ...) #1

; Function Attrs: argmemonly nocallback nofree nounwind willreturn
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #2

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nocallback nofree nounwind willreturn }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.1-++20200519100828+f79cd71e145-1~exp1~20200519201452.38 "}
