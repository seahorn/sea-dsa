; ModuleID = 'sea-dsa/tools/tests/callgraph/test_callgraph.ll'
source_filename = "test_callgraph.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define i32 @add(i32 %arg, i32 %arg1) #0 {
bb:
  %tmp = alloca i32, align 4
  %tmp2 = alloca i32, align 4
  store i32 %arg, i32* %tmp, align 4
  store i32 %arg1, i32* %tmp2, align 4
  %tmp3 = load i32, i32* %tmp, align 4
  %tmp4 = load i32, i32* %tmp2, align 4
  %tmp5 = add nsw i32 %tmp3, %tmp4
  ret i32 %tmp5
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @sub(i32 %arg, i32 %arg1) #0 {
bb:
  %tmp = alloca i32, align 4
  %tmp2 = alloca i32, align 4
  store i32 %arg, i32* %tmp, align 4
  store i32 %arg1, i32* %tmp2, align 4
  %tmp3 = load i32, i32* %tmp, align 4
  %tmp4 = load i32, i32* %tmp2, align 4
  %tmp5 = sub nsw i32 %tmp3, %tmp4
  ret i32 %tmp5
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @use(i32 (i32, i32)* %arg) #0 {
bb:
  %tmp = alloca i32 (i32, i32)*, align 8
  %tmp1 = alloca i32, align 4
  store i32 (i32, i32)* %arg, i32 (i32, i32)** %tmp, align 8
  %tmp2 = load i32 (i32, i32)*, i32 (i32, i32)** %tmp, align 8
  %callsite_in_use = call i32 %tmp2(i32 10, i32 20)
  store i32 %callsite_in_use, i32* %tmp1, align 4
  %tmp4 = load i32, i32* %tmp1, align 4
  ret i32 %tmp4
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @main() #0 {
bb:
  %tmp = alloca i32, align 4
  %tmp1 = alloca i32, align 4
  %tmp2 = alloca i32, align 4
  store i32 0, i32* %tmp, align 4
  %callsite_in_main_1 = call i32 @use(i32 (i32, i32)* @add)
  store i32 %callsite_in_main_1, i32* %tmp1, align 4
  %callsite_in_main_2 = call i32 @use(i32 (i32, i32)* @sub)
  store i32 %callsite_in_main_2, i32* %tmp2, align 4
  %tmp5 = load i32, i32* %tmp1, align 4
  %tmp6 = load i32, i32* %tmp2, align 4
  %tmp7 = add nsw i32 %tmp5, %tmp6
  ret i32 %tmp7
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 6.0.0-1ubuntu2 (tags/RELEASE_600/final)"}
