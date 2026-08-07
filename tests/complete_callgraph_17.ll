; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_17.ll
; RUN: %cmp-graphs %tests/complete_callgraph_17.dot %T/complete_callgraph_17.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_17.cpp'
source_filename = "complete_callgraph_17.cpp"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

@_ZTV12DerivedClass = linkonce_odr unnamed_addr constant { [5 x ptr] } { [5 x ptr] [ptr null, ptr @_ZTI12DerivedClass, ptr @_ZN12DerivedClassD1Ev, ptr @_ZN12DerivedClassD0Ev, ptr @_ZN12DerivedClass4FuncEv] }, align 8
@_ZTVN10__cxxabiv120__si_class_type_infoE = external global ptr
@_ZTS12DerivedClass = linkonce_odr hidden constant [15 x i8] c"12DerivedClass\00", align 1
@_ZTVN10__cxxabiv117__class_type_infoE = external global ptr
@_ZTS9BaseClass = linkonce_odr hidden constant [11 x i8] c"9BaseClass\00", align 1
@_ZTI9BaseClass = linkonce_odr hidden constant { ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv117__class_type_infoE, i64 2), ptr inttoptr (i64 add (i64 ptrtoint (ptr @_ZTS9BaseClass to i64), i64 -9223372036854775808) to ptr) }, align 8
@_ZTI12DerivedClass = linkonce_odr hidden constant { ptr, ptr, ptr } { ptr getelementptr inbounds (ptr, ptr @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2), ptr inttoptr (i64 add (i64 ptrtoint (ptr @_ZTS12DerivedClass to i64), i64 -9223372036854775808) to ptr), ptr @_ZTI9BaseClass }, align 8
@_ZTV9BaseClass = linkonce_odr unnamed_addr constant { [5 x ptr] } { [5 x ptr] [ptr null, ptr @_ZTI9BaseClass, ptr @_ZN9BaseClassD1Ev, ptr @_ZN9BaseClassD0Ev, ptr @_ZN9BaseClass4FuncEv] }, align 8

; Function Attrs: noinline norecurse optnone ssp uwtable
define i32 @main(i32 %0, ptr %1) #0 personality ptr @__gxx_personality_v0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca ptr, align 8
  %6 = alloca ptr, align 8
  %7 = alloca ptr, align 8
  %8 = alloca i32, align 4
  store i32 0, ptr %3, align 4
  store i32 %0, ptr %4, align 4
  store ptr %1, ptr %5, align 8
  %9 = call noalias nonnull ptr @_Znwm(i64 8) #5
  %10 = bitcast ptr %9 to ptr
  %11 = invoke ptr @_ZN12DerivedClassC1Ev(ptr %10)
          to label %12 unwind label %20

12:                                               ; preds = %2
  %13 = bitcast ptr %10 to ptr
  store ptr %13, ptr %6, align 8
  %14 = load ptr, ptr %6, align 8
  %15 = bitcast ptr %14 to ptr
  %16 = load ptr, ptr %15, align 8
  %17 = getelementptr inbounds ptr, ptr %16, i64 2
  %18 = load ptr, ptr %17, align 8
  %19 = call i32 %18(ptr %14)
  ret i32 %19

20:                                               ; preds = %2
  %21 = landingpad { ptr, i32 }
          cleanup
  %22 = extractvalue { ptr, i32 } %21, 0
  store ptr %22, ptr %7, align 8
  %23 = extractvalue { ptr, i32 } %21, 1
  store i32 %23, ptr %8, align 4
  call void @_ZdlPv(ptr %9) #6
  br label %24

24:                                               ; preds = %20
  %25 = load ptr, ptr %7, align 8
  %26 = load i32, ptr %8, align 4
  %27 = insertvalue { ptr, i32 } undef, ptr %25, 0
  %28 = insertvalue { ptr, i32 } %27, i32 %26, 1
  resume { ptr, i32 } %28
}

; Function Attrs: nobuiltin allocsize(0)
declare nonnull ptr @_Znwm(i64) #1

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr ptr @_ZN12DerivedClassC1Ev(ptr returned %0) unnamed_addr #2 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = call ptr @_ZN12DerivedClassC2Ev(ptr %3)
  ret ptr %3
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nobuiltin nounwind
declare void @_ZdlPv(ptr) #3

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr ptr @_ZN12DerivedClassC2Ev(ptr returned %0) unnamed_addr #2 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = bitcast ptr %3 to ptr
  %5 = call ptr @_ZN9BaseClassC2Ev(ptr %4)
  %6 = bitcast ptr %3 to ptr
  store ptr getelementptr inbounds ({ [5 x ptr] }, ptr @_ZTV12DerivedClass, i32 0, inrange i32 0, i32 2), ptr %6, align 8
  ret ptr %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr ptr @_ZN9BaseClassC2Ev(ptr returned %0) unnamed_addr #4 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = bitcast ptr %3 to ptr
  store ptr getelementptr inbounds ({ [5 x ptr] }, ptr @_ZTV9BaseClass, i32 0, inrange i32 0, i32 2), ptr %4, align 8
  ret ptr %3
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr ptr @_ZN12DerivedClassD1Ev(ptr returned %0) unnamed_addr #2 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = call ptr @_ZN12DerivedClassD2Ev(ptr %3)
  ret ptr %3
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN12DerivedClassD0Ev(ptr %0) unnamed_addr #2 align 2 personality ptr @__gxx_personality_v0 {
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  %4 = alloca i32, align 4
  store ptr %0, ptr %2, align 8
  %5 = load ptr, ptr %2, align 8
  %6 = invoke ptr @_ZN12DerivedClassD1Ev(ptr %5)
          to label %7 unwind label %9

7:                                                ; preds = %1
  %8 = bitcast ptr %5 to ptr
  call void @_ZdlPv(ptr %8) #6
  ret void

9:                                                ; preds = %1
  %10 = landingpad { ptr, i32 }
          cleanup
  %11 = extractvalue { ptr, i32 } %10, 0
  store ptr %11, ptr %3, align 8
  %12 = extractvalue { ptr, i32 } %10, 1
  store i32 %12, ptr %4, align 4
  %13 = bitcast ptr %5 to ptr
  call void @_ZdlPv(ptr %13) #6
  br label %14

14:                                               ; preds = %9
  %15 = load ptr, ptr %3, align 8
  %16 = load i32, ptr %4, align 4
  %17 = insertvalue { ptr, i32 } undef, ptr %15, 0
  %18 = insertvalue { ptr, i32 } %17, i32 %16, 1
  resume { ptr, i32 } %18
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i32 @_ZN12DerivedClass4FuncEv(ptr %0) unnamed_addr #4 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  ret i32 1
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr ptr @_ZN9BaseClassD1Ev(ptr returned %0) unnamed_addr #2 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = call ptr @_ZN9BaseClassD2Ev(ptr %3)
  ret ptr %3
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN9BaseClassD0Ev(ptr %0) unnamed_addr #2 align 2 personality ptr @__gxx_personality_v0 {
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  %4 = alloca i32, align 4
  store ptr %0, ptr %2, align 8
  %5 = load ptr, ptr %2, align 8
  %6 = invoke ptr @_ZN9BaseClassD1Ev(ptr %5)
          to label %7 unwind label %9

7:                                                ; preds = %1
  %8 = bitcast ptr %5 to ptr
  call void @_ZdlPv(ptr %8) #6
  ret void

9:                                                ; preds = %1
  %10 = landingpad { ptr, i32 }
          cleanup
  %11 = extractvalue { ptr, i32 } %10, 0
  store ptr %11, ptr %3, align 8
  %12 = extractvalue { ptr, i32 } %10, 1
  store i32 %12, ptr %4, align 4
  %13 = bitcast ptr %5 to ptr
  call void @_ZdlPv(ptr %13) #6
  br label %14

14:                                               ; preds = %9
  %15 = load ptr, ptr %3, align 8
  %16 = load i32, ptr %4, align 4
  %17 = insertvalue { ptr, i32 } undef, ptr %15, 0
  %18 = insertvalue { ptr, i32 } %17, i32 %16, 1
  resume { ptr, i32 } %18
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i32 @_ZN9BaseClass4FuncEv(ptr %0) unnamed_addr #4 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  ret i32 0
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr ptr @_ZN9BaseClassD2Ev(ptr returned %0) unnamed_addr #4 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  ret ptr %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr ptr @_ZN12DerivedClassD2Ev(ptr returned %0) unnamed_addr #4 align 2 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = bitcast ptr %3 to ptr
  %5 = call ptr @_ZN9BaseClassD2Ev(ptr %4)
  ret ptr %3
}

attributes #0 = { noinline norecurse optnone ssp uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #1 = { nobuiltin allocsize(0) "frame-pointer"="non-leaf" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #2 = { noinline optnone ssp uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #3 = { nobuiltin nounwind "frame-pointer"="non-leaf" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #4 = { noinline nounwind optnone ssp uwtable "frame-pointer"="non-leaf" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="__chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="apple-m1" "target-features"="+aes,+crc,+crypto,+dotprod,+fp-armv8,+fp16fml,+fullfp16,+lse,+neon,+ras,+rcpc,+rdm,+sha2,+sha3,+sm4,+v8.5a,+zcm,+zcz" }
attributes #5 = { builtin allocsize(0) }
attributes #6 = { builtin nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 13, i32 1]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"branch-target-enforcement", i32 0}
!3 = !{i32 8, !"sign-return-address", i32 0}
!4 = !{i32 8, !"sign-return-address-all", i32 0}
!5 = !{i32 8, !"sign-return-address-with-bkey", i32 0}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 1}
!8 = !{i32 7, !"frame-pointer", i32 1}
!9 = !{!"Apple clang version 14.0.0 (clang-1400.0.29.202)"}
