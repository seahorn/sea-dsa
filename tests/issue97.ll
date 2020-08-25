; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/issue97.ll
; RUN: %cmp-graphs %tests/issue97.callgraph.dot %T/issue97.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$


; ModuleID = 'issue97.c'
source_filename = "issue97.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.15.0"

%struct.command = type { i8*, void (...)*, i8 }

@state = global i32 0, align 4
@global = global i8* null, align 8
@.str = private unnamed_addr constant [3 x i8] c"c1\00", align 1
@.str.1 = private unnamed_addr constant [3 x i8] c"c2\00", align 1
@commands = constant [2 x %struct.command] [%struct.command { i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i32 0, i32 0), void (...)* bitcast (void ()* @c1 to void (...)*), i8 0 }, %struct.command { i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.1, i32 0, i32 0), void (...)* bitcast (void ()* @c2 to void (...)*), i8 1 }], align 16

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @parse_input(i8* %0) #0 {
  %2 = alloca i8*, align 8
  %3 = alloca i32, align 4
  store i8* %0, i8** %2, align 8
  store i32 0, i32* %3, align 4
  br label %4

4:                                                ; preds = %33, %1
  %5 = load i32, i32* %3, align 4
  %6 = icmp slt i32 %5, 2
  br i1 %6, label %7, label %36

7:                                                ; preds = %4
  %8 = load i32, i32* %3, align 4
  %9 = sext i32 %8 to i64
  %10 = getelementptr inbounds [2 x %struct.command], [2 x %struct.command]* @commands, i64 0, i64 %9
  %11 = getelementptr inbounds %struct.command, %struct.command* %10, i32 0, i32 0
  %12 = load i8*, i8** %11, align 8
  %13 = load i8*, i8** %2, align 8
  %14 = call i32 @strcmp(i8* %12, i8* %13) #3
  %15 = icmp eq i32 %14, 0
  br i1 %15, label %16, label %32

16:                                               ; preds = %7
  %17 = load i32, i32* @state, align 4
  %18 = load i32, i32* %3, align 4
  %19 = sext i32 %18 to i64
  %20 = getelementptr inbounds [2 x %struct.command], [2 x %struct.command]* @commands, i64 0, i64 %19
  %21 = getelementptr inbounds %struct.command, %struct.command* %20, i32 0, i32 2
  %22 = load i8, i8* %21, align 8
  %23 = sext i8 %22 to i32
  %24 = icmp sge i32 %17, %23
  br i1 %24, label %25, label %31

25:                                               ; preds = %16
  %26 = load i32, i32* %3, align 4
  %27 = sext i32 %26 to i64
  %28 = getelementptr inbounds [2 x %struct.command], [2 x %struct.command]* @commands, i64 0, i64 %27
  %29 = getelementptr inbounds %struct.command, %struct.command* %28, i32 0, i32 1
  %30 = load void (...)*, void (...)** %29, align 8
  call void (...) %30()
  br label %36

31:                                               ; preds = %16
  br label %32

32:                                               ; preds = %31, %7
  br label %33

33:                                               ; preds = %32
  %34 = load i32, i32* %3, align 4
  %35 = add nsw i32 %34, 1
  store i32 %35, i32* %3, align 4
  br label %4

36:                                               ; preds = %25, %4
  ret void
}

; Function Attrs: nounwind readonly
declare i32 @strcmp(i8*, i8*) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @c1() #0 {
  %1 = alloca i8*, align 8
  %2 = call noalias i8* @malloc(i32 1) #4
  store i8* %2, i8** %1, align 8
  %3 = load i8*, i8** %1, align 8
  %4 = icmp ne i8* %3, null
  br i1 %4, label %6, label %5

5:                                                ; preds = %0
  br label %13

6:                                                ; preds = %0
  %7 = load i8*, i8** @global, align 8
  %8 = icmp ne i8* %7, null
  br i1 %8, label %9, label %11

9:                                                ; preds = %6
  %10 = load i8*, i8** @global, align 8
  call void @free(i8* %10) #4
  br label %11

11:                                               ; preds = %9, %6
  %12 = load i8*, i8** %1, align 8
  store i8* %12, i8** @global, align 8
  store i32 1, i32* @state, align 4
  br label %13

13:                                               ; preds = %11, %5
  ret void
}

; Function Attrs: nounwind
declare noalias i8* @malloc(i32) #2

; Function Attrs: nounwind
declare void @free(i8*) #2

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @c2() #0 {
  %1 = alloca i8*, align 8
  store i8* null, i8** %1, align 8
  %2 = load i8*, i8** %1, align 8
  %3 = icmp ne i8* %2, null
  br i1 %3, label %4, label %7

4:                                                ; preds = %0
  %5 = load i8*, i8** @global, align 8
  %6 = icmp ne i8* %5, null
  br i1 %6, label %8, label %7

7:                                                ; preds = %4, %0
  br label %11

8:                                                ; preds = %4
  %9 = load i8*, i8** %1, align 8
  call void @free(i8* %9) #4
  %10 = load i8*, i8** @global, align 8
  call void @free(i8* %10) #4
  store i32 0, i32* @state, align 4
  br label %11

11:                                               ; preds = %8, %7
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  call void @parse_input(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0))
  call void @parse_input(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0))
  call void @parse_input(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.1, i64 0, i64 0))
  ret i32 0
}

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind readonly }
attributes #4 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 10.0.0 "}
