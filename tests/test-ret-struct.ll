; RUN: %seadsa  %butd_dsa --sea-dsa-dot %s --sea-dsa-dot-outdir=%T/test-ret-struct.ll
; RUN: %cmp-graphs %tests/test-ret-struct.c.main.mem.dot %T/test-ret-struct.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$


; ModuleID = 'test-shaobo.1.c'
source_filename = "test-shaobo.1.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

%struct.S = type { i32*, i64 }

@A = common global i32* null, align 8
@__func__.main = private unnamed_addr constant [5 x i8] c"main\00", align 1
@.str = private unnamed_addr constant [16 x i8] c"test-shaobo.1.c\00", align 1
@.str.1 = private unnamed_addr constant [12 x i8] c"*(y.a) == 1\00", align 1

; Function Attrs: noinline nounwind optnone ssp uwtable
define { i32*, i64 } @foo() #0 {
  %1 = alloca %struct.S, align 8
  %2 = alloca %struct.S, align 8
  %3 = getelementptr inbounds %struct.S, %struct.S* %2, i32 0, i32 0
  %4 = load i32*, i32** @A, align 8
  store i32* %4, i32** %3, align 8
  %5 = getelementptr inbounds %struct.S, %struct.S* %2, i32 0, i32 1
  store i64 2, i64* %5, align 8
  %6 = bitcast %struct.S* %1 to i8*
  %7 = bitcast %struct.S* %2 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* %6, i8* %7, i64 16, i32 8, i1 false)
  %8 = bitcast %struct.S* %1 to { i32*, i64 }*
  %9 = load { i32*, i64 }, { i32*, i64 }* %8, align 8
  ret { i32*, i64 } %9
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca %struct.S, align 8
  store i32 0, i32* %1, align 4
  store i32 1, i32* %2, align 4
  store i32* %2, i32** @A, align 8
  %4 = call { i32*, i64 } @foo()
  %5 = bitcast %struct.S* %3 to { i32*, i64 }*
  %6 = getelementptr inbounds { i32*, i64 }, { i32*, i64 }* %5, i32 0, i32 0
  %7 = extractvalue { i32*, i64 } %4, 0
  store i32* %7, i32** %6, align 8
  %8 = getelementptr inbounds { i32*, i64 }, { i32*, i64 }* %5, i32 0, i32 1
  %9 = extractvalue { i32*, i64 } %4, 1
  store i64 %9, i64* %8, align 8
  %10 = getelementptr inbounds %struct.S, %struct.S* %3, i32 0, i32 0
  %11 = load i32*, i32** %10, align 8
  %12 = load i32, i32* %11, align 4
  %13 = icmp eq i32 %12, 1
  %14 = xor i1 %13, true
  %15 = zext i1 %14 to i32
  %16 = sext i32 %15 to i64
  %17 = icmp ne i64 %16, 0
  br i1 %17, label %18, label %20

; <label>:18:                                     ; preds = %0
  call void @__assert_rtn(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @__func__.main, i32 0, i32 0), i8* getelementptr inbounds ([16 x i8], [16 x i8]* @.str, i32 0, i32 0), i32 20, i8* getelementptr inbounds ([12 x i8], [12 x i8]* @.str.1, i32 0, i32 0)) #3
  unreachable
                                                  ; No predecessors!
  br label %21

; <label>:20:                                     ; preds = %0
  br label %21

; <label>:21:                                     ; preds = %20, %19
  ret i32 0
}

; Function Attrs: noreturn
declare void @__assert_rtn(i8*, i8*, i32, i8*) #2

attributes #0 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { noreturn "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="true" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { noreturn }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
