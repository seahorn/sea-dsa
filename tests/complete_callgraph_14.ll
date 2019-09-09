; RUN: %seadsa %s --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_14.ll
; RUN: %cmp-graphs %tests/complete_callgraph_14.dot %T/complete_callgraph_14.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_14.bc'
source_filename = "complete_callgraph_14.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

%struct.Struct = type { i32, i32, i32 (i32)** }

@hash = common global i32 (i32)* null, align 8
@hash_ptr = global i32 (i32)** @hash, align 8

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @jenkins_hash(i32) #0 {
  %2 = alloca i32, align 4
  store i32 %0, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  ret i32 %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @MurmurHash3_hash(i32) #0 {
  %2 = alloca i32, align 4
  store i32 %0, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  ret i32 %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @hash_init() #0 {
  %1 = call i32 (...) @nd()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %3, label %5

; <label>:3:                                      ; preds = %0
  %4 = load i32 (i32)**, i32 (i32)*** @hash_ptr, align 8
  store i32 (i32)* @jenkins_hash, i32 (i32)** %4, align 8
  br label %7

; <label>:5:                                      ; preds = %0
  %6 = load i32 (i32)**, i32 (i32)*** @hash_ptr, align 8
  store i32 (i32)* @MurmurHash3_hash, i32 (i32)** %6, align 8
  br label %7

; <label>:7:                                      ; preds = %5, %3
  ret void
}

declare i32 @nd(...) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @init(%struct.Struct*, void ()*) #0 {
  %3 = alloca %struct.Struct*, align 8
  %4 = alloca void ()*, align 8
  store %struct.Struct* %0, %struct.Struct** %3, align 8
  store void ()* %1, void ()** %4, align 8
  %5 = load void ()*, void ()** %4, align 8
  call void %5()
  %6 = load i32 (i32)*, i32 (i32)** @hash, align 8
  %7 = load %struct.Struct*, %struct.Struct** %3, align 8
  %8 = getelementptr inbounds %struct.Struct, %struct.Struct* %7, i32 0, i32 2
  %9 = load i32 (i32)**, i32 (i32)*** %8, align 8
  store i32 (i32)* %6, i32 (i32)** %9, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca %struct.Struct*, align 8
  store i32 0, i32* %1, align 4
  %3 = call i8* @malloc(i64 16) #3
  %4 = bitcast i8* %3 to %struct.Struct*
  store %struct.Struct* %4, %struct.Struct** %2, align 8
  %5 = load %struct.Struct*, %struct.Struct** %2, align 8
  %6 = getelementptr inbounds %struct.Struct, %struct.Struct* %5, i32 0, i32 0
  store i32 0, i32* %6, align 8
  %7 = load %struct.Struct*, %struct.Struct** %2, align 8
  %8 = getelementptr inbounds %struct.Struct, %struct.Struct* %7, i32 0, i32 1
  store i32 0, i32* %8, align 4
  %9 = call i8* @malloc(i64 8) #3
  %10 = bitcast i8* %9 to i32 (i32)**
  %11 = load %struct.Struct*, %struct.Struct** %2, align 8
  %12 = getelementptr inbounds %struct.Struct, %struct.Struct* %11, i32 0, i32 2
  store i32 (i32)** %10, i32 (i32)*** %12, align 8
  %13 = load %struct.Struct*, %struct.Struct** %2, align 8
  call void @init(%struct.Struct* %13, void ()* @hash_init)
  %14 = load %struct.Struct*, %struct.Struct** %2, align 8
  %15 = getelementptr inbounds %struct.Struct, %struct.Struct* %14, i32 0, i32 2
  %16 = load i32 (i32)**, i32 (i32)*** %15, align 8
  %17 = load i32 (i32)*, i32 (i32)** %16, align 8
  %18 = call i32 %17(i32 8)
  ret i32 %18
}

; Function Attrs: allocsize(0)
declare i8* @malloc(i64) #2

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { allocsize(0) "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { allocsize(0) }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
