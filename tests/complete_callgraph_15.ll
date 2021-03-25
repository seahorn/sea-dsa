; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_15.ll
; RUN: %cmp-graphs %tests/complete_callgraph_15.dot %T/complete_callgraph_15.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_15.bc'
source_filename = "complete_callgraph_15.c"
target datalayout = "e-m:o-p:32:32-p270:32:32-p271:32:32-p272:64:64-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.15.0"

%struct.test_handler = type { i32 (...)* }

@handle_var = global %struct.test_handler { i32 (...)* bitcast (i32 ()* @foo to i32 (...)*) }, align 4

; Function Attrs: nounwind ssp uwtable
define i32 @foo() #0 {
  %1 = alloca i32, align 4
  %2 = bitcast i32* %1 to i8*
  store i32 0, i32* %1, align 4, !tbaa !4
  %3 = load i32, i32* %1, align 4, !tbaa !4
  %4 = bitcast i32* %1 to i8*
  ret i32 %3
}

; Function Attrs: nounwind ssp uwtable
define %struct.test_handler* @return_handler_struct() #0 {
  ret %struct.test_handler* @handle_var
}

; Function Attrs: nounwind ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = call %struct.test_handler* @return_handler_struct()
  %3 = getelementptr inbounds %struct.test_handler, %struct.test_handler* %2, i32 0, i32 0
  %4 = load i32 (...)*, i32 (...)** %3, align 4, !tbaa !8
  %5 = bitcast i32 (...)* %4 to i32 ()*
  %6 = call i32 %5()
  %7 = icmp eq i32 %6, 0
  br i1 %7, label %9, label %8

8:                                                ; preds = %0
  br label %9

9:                                                ; preds = %8, %0
  %10 = phi i1 [ true, %0 ], [ false, %8 ]
  %11 = zext i1 %10 to i32
  ret i32 0
}

attributes #0 = { nounwind ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind willreturn }
attributes #2 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nounwind }

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
!8 = !{!9, !10, i64 0}
!9 = !{!"test_handler", !10, i64 0}
!10 = !{!"any pointer", !6, i64 0}
