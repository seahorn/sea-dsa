; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_16.ll
; RUN: %cmp-graphs %tests/complete_callgraph_16.dot %T/complete_callgraph_16.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = './test_devirt.bc'
source_filename = "./test_devirt.c"
target datalayout = "e-m:o-p:32:32-p270:32:32-p271:32:32-p272:64:64-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.15.0"

%struct.test_handler = type { %struct.test_sub, i32 (...)* }
%struct.test_sub = type { i32 }

; Function Attrs: nounwind ssp uwtable
define i32 @foo() #0 {
  %1 = alloca i32, align 4
  %2 = bitcast i32* %1 to i8*
  store i32 0, i32* %1, align 4, !tbaa !4
  %3 = load i32, i32* %1, align 4, !tbaa !4
  %4 = icmp eq i32 %3, 0
  br i1 %4, label %6, label %5

5:                                                ; preds = %0
  br label %6

6:                                                ; preds = %5, %0
  %7 = phi i1 [ true, %0 ], [ false, %5 ]
  %8 = zext i1 %7 to i32
  %9 = load i32, i32* %1, align 4, !tbaa !4
  %10 = bitcast i32* %1 to i8*
  ret i32 %9
}

; Function Attrs: nounwind ssp uwtable
define %struct.test_handler* @return_handler_struct() #0 {
  %1 = alloca %struct.test_handler*, align 4
  %2 = bitcast %struct.test_handler** %1 to i8*
  %3 = call i8* @malloc(i32 8) #5
  %4 = bitcast i8* %3 to %struct.test_handler*
  store %struct.test_handler* %4, %struct.test_handler** %1, align 4, !tbaa !8
  %5 = load %struct.test_handler*, %struct.test_handler** %1, align 4, !tbaa !8
  %6 = getelementptr inbounds %struct.test_handler, %struct.test_handler* %5, i32 0, i32 1
  store i32 (...)* bitcast (i32 ()* @foo to i32 (...)*), i32 (...)** %6, align 4, !tbaa !10
  %7 = load %struct.test_handler*, %struct.test_handler** %1, align 4, !tbaa !8
  %8 = bitcast %struct.test_handler** %1 to i8*
  ret %struct.test_handler* %7
}

; Function Attrs: allocsize(0)
declare i8* @malloc(i32) #3

; Function Attrs: nounwind ssp uwtable
define i32 @main(i32 %0, i8** %1) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 4
  %6 = alloca %struct.test_handler*, align 4
  %7 = alloca %struct.test_sub*, align 4
  %8 = alloca %struct.test_handler*, align 4
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4, !tbaa !4
  store i8** %1, i8*** %5, align 4, !tbaa !8
  %9 = bitcast %struct.test_handler** %6 to i8*
  %10 = call %struct.test_handler* @return_handler_struct()
  store %struct.test_handler* %10, %struct.test_handler** %6, align 4, !tbaa !8
  %11 = bitcast %struct.test_sub** %7 to i8*
  %12 = load %struct.test_handler*, %struct.test_handler** %6, align 4, !tbaa !8
  %13 = bitcast %struct.test_handler* %12 to %struct.test_sub*
  store %struct.test_sub* %13, %struct.test_sub** %7, align 4, !tbaa !8
  %14 = bitcast %struct.test_handler** %8 to i8*
  %15 = load %struct.test_sub*, %struct.test_sub** %7, align 4, !tbaa !8
  %16 = call %struct.test_handler* @to_test_handler(%struct.test_sub* %15)
  store %struct.test_handler* %16, %struct.test_handler** %8, align 4, !tbaa !8
  %17 = load %struct.test_handler*, %struct.test_handler** %8, align 4, !tbaa !8
  %18 = getelementptr inbounds %struct.test_handler, %struct.test_handler* %17, i32 0, i32 1
  %19 = load i32 (...)*, i32 (...)** %18, align 4, !tbaa !10
  %20 = bitcast i32 (...)* %19 to i32 ()*
  %21 = call i32 %20()
  %22 = icmp eq i32 %21, 0
  br i1 %22, label %24, label %23

23:                                               ; preds = %2
  br label %24

24:                                               ; preds = %23, %2
  %25 = phi i1 [ true, %2 ], [ false, %23 ]
  %26 = zext i1 %25 to i32
  %27 = bitcast %struct.test_handler** %8 to i8*
  %28 = bitcast %struct.test_sub** %7 to i8*
  %29 = bitcast %struct.test_handler** %6 to i8*
  ret i32 0
}

; Function Attrs: nounwind ssp uwtable
define internal %struct.test_handler* @to_test_handler(%struct.test_sub* %0) #0 {
  %2 = alloca %struct.test_sub*, align 4
  store %struct.test_sub* %0, %struct.test_sub** %2, align 4, !tbaa !8
  %3 = load %struct.test_sub*, %struct.test_sub** %2, align 4, !tbaa !8
  %4 = icmp ne %struct.test_sub* %3, null
  br i1 %4, label %6, label %5

5:                                                ; preds = %1
  br label %6

6:                                                ; preds = %5, %1
  %7 = phi i1 [ true, %1 ], [ false, %5 ]
  %8 = zext i1 %7 to i32
  %9 = load %struct.test_sub*, %struct.test_sub** %2, align 4, !tbaa !8
  %10 = ptrtoint %struct.test_sub* %9 to i32
  %11 = sub i32 %10, 0
  %12 = inttoptr i32 %11 to %struct.test_handler*
  ret %struct.test_handler* %12
}

attributes #0 = { nounwind ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind willreturn }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { allocsize(0) "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nounwind }
attributes #5 = { allocsize(0) }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"NumRegisterParameters", i32 0}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{!"clang version 10.0.0 "}
!4 = !{!5, !5, i64 0}
!5 = !{!"int", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C/C++ TBAA"}
!8 = !{!9, !9, i64 0}
!9 = !{!"any pointer", !6, i64 0}
!10 = !{!11, !9, i64 4}
!11 = !{!"test_handler", !12, i64 0, !9, i64 4}
!12 = !{!"test_sub", !5, i64 0}
