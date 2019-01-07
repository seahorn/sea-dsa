; ModuleID = 'test_callgraph_2.c'
source_filename = "test_callgraph_2.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.foo = type { i32*, i32 (i32, i32)* }

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define i32 @add(i32, i32) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  store i32 %1, i32* %4, align 4
  %5 = load i32, i32* %3, align 4
  %6 = load i32, i32* %4, align 4
  %7 = add nsw i32 %5, %6
  ret i32 %7
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @sub(i32, i32) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  store i32 %1, i32* %4, align 4
  %5 = load i32, i32* %3, align 4
  %6 = load i32, i32* %4, align 4
  %7 = sub nsw i32 %5, %6
  ret i32 %7
}

; Function Attrs: noinline nounwind optnone uwtable
define i32 @use_2(%struct.foo*) #0 {
  %2 = alloca %struct.foo*, align 8
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store %struct.foo* %0, %struct.foo** %2, align 8
  %5 = load %struct.foo*, %struct.foo** %2, align 8
  %6 = getelementptr inbounds %struct.foo, %struct.foo* %5, i32 0, i32 0
  %7 = load i32*, i32** %6, align 8
  %8 = load i32, i32* %7, align 4
  store i32 %8, i32* %3, align 4
  %9 = load i32, i32* %3, align 4
  %10 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %9)
  %11 = load %struct.foo*, %struct.foo** %2, align 8
  %12 = getelementptr inbounds %struct.foo, %struct.foo* %11, i32 0, i32 1
  %13 = load i32 (i32, i32)*, i32 (i32, i32)** %12, align 8
  %14 = call i32 %13(i32 100, i32 200)
  store i32 %14, i32* %4, align 4
  %15 = load i32, i32* %4, align 4
  ret i32 %15
}

declare i32 @printf(i8*, ...) #1

; Function Attrs: noinline nounwind optnone uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32*, align 8
  %3 = alloca %struct.foo, align 8
  %4 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %5 = load i32*, i32** %2, align 8
  store i32 15, i32* %5, align 4
  %6 = getelementptr inbounds %struct.foo, %struct.foo* %3, i32 0, i32 0
  %7 = load i32*, i32** %2, align 8
  store i32* %7, i32** %6, align 8
  %8 = getelementptr inbounds %struct.foo, %struct.foo* %3, i32 0, i32 1
  store i32 (i32, i32)* @add, i32 (i32, i32)** %8, align 8
  %9 = call i32 @use_2(%struct.foo* %3)
  store i32 %9, i32* %4, align 4
  %10 = load i32, i32* %4, align 4
  %11 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i32 0, i32 0), i32 %10)
  %12 = load i32, i32* %4, align 4
  ret i32 %12
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 6.0.0-1ubuntu2 (tags/RELEASE_600/final)"}
