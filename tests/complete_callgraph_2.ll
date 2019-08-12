; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_2.ll
; RUN: %cmp-graphs %tests/complete_callgraph_2.dot %T/complete_callgraph_2.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_2.bc'
source_filename = "complete_callgraph_2.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

%struct.class_t = type { {}*, {}* }

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @foo(%struct.class_t*, i32) #0 {
  %3 = alloca i32, align 4
  %4 = alloca %struct.class_t*, align 8
  %5 = alloca i32, align 4
  store %struct.class_t* %0, %struct.class_t** %4, align 8
  store i32 %1, i32* %5, align 4
  %6 = load i32, i32* %5, align 4
  %7 = icmp sgt i32 %6, 10
  br i1 %7, label %8, label %17

; <label>:8:                                      ; preds = %2
  %9 = load %struct.class_t*, %struct.class_t** %4, align 8
  %10 = getelementptr inbounds %struct.class_t, %struct.class_t* %9, i32 0, i32 1
  %11 = bitcast {}** %10 to i32 (%struct.class_t*, i32)**
  %12 = load i32 (%struct.class_t*, i32)*, i32 (%struct.class_t*, i32)** %11, align 8
  %13 = load %struct.class_t*, %struct.class_t** %4, align 8
  %14 = load i32, i32* %5, align 4
  %15 = add nsw i32 %14, 1
  %16 = call i32 %12(%struct.class_t* %13, i32 %15)
  store i32 %16, i32* %3, align 4
  br label %19

; <label>:17:                                     ; preds = %2
  %18 = load i32, i32* %5, align 4
  store i32 %18, i32* %3, align 4
  br label %19

; <label>:19:                                     ; preds = %17, %8
  %20 = load i32, i32* %3, align 4
  ret i32 %20
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @bar(%struct.class_t*, i32) #0 {
  %3 = alloca i32, align 4
  %4 = alloca %struct.class_t*, align 8
  %5 = alloca i32, align 4
  store %struct.class_t* %0, %struct.class_t** %4, align 8
  store i32 %1, i32* %5, align 4
  %6 = load i32, i32* %5, align 4
  %7 = icmp slt i32 %6, 100
  br i1 %7, label %8, label %11

; <label>:8:                                      ; preds = %2
  %9 = load i32, i32* %5, align 4
  %10 = add nsw i32 %9, 10
  store i32 %10, i32* %3, align 4
  br label %14

; <label>:11:                                     ; preds = %2
  %12 = load i32, i32* %5, align 4
  %13 = sub nsw i32 %12, 5
  store i32 %13, i32* %3, align 4
  br label %14

; <label>:14:                                     ; preds = %11, %8
  %15 = load i32, i32* %3, align 4
  ret i32 %15
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca %struct.class_t, align 8
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %4 = getelementptr inbounds %struct.class_t, %struct.class_t* %2, i32 0, i32 0
  %5 = bitcast {}** %4 to i32 (%struct.class_t*, i32)**
  store i32 (%struct.class_t*, i32)* @foo, i32 (%struct.class_t*, i32)** %5, align 8
  %6 = getelementptr inbounds %struct.class_t, %struct.class_t* %2, i32 0, i32 1
  %7 = bitcast {}** %6 to i32 (%struct.class_t*, i32)**
  store i32 (%struct.class_t*, i32)* @bar, i32 (%struct.class_t*, i32)** %7, align 8
  %8 = call i32 @foo(%struct.class_t* %2, i32 42)
  store i32 %8, i32* %3, align 4
  ret i32 0
}

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
