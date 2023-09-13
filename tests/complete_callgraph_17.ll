; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_17.ll
; RUN: %cmp-graphs %tests/complete_callgraph_17.dot %T/complete_callgraph_17.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'complete_callgraph_17.cpp'
source_filename = "complete_callgraph_17.cpp"
target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"
target triple = "arm64-apple-macosx13.0.0"

%class.BaseClass = type { i32 (...)** }
%class.DerivedClass = type { %class.BaseClass }

@_ZTV12DerivedClass = linkonce_odr unnamed_addr constant { [5 x i8*] } { [5 x i8*] [i8* null, i8* bitcast ({ i8*, i8*, i8* }* @_ZTI12DerivedClass to i8*), i8* bitcast (%class.DerivedClass* (%class.DerivedClass*)* @_ZN12DerivedClassD1Ev to i8*), i8* bitcast (void (%class.DerivedClass*)* @_ZN12DerivedClassD0Ev to i8*), i8* bitcast (i32 (%class.DerivedClass*)* @_ZN12DerivedClass4FuncEv to i8*)] }, align 8
@_ZTVN10__cxxabiv120__si_class_type_infoE = external global i8*
@_ZTS12DerivedClass = linkonce_odr hidden constant [15 x i8] c"12DerivedClass\00", align 1
@_ZTVN10__cxxabiv117__class_type_infoE = external global i8*
@_ZTS9BaseClass = linkonce_odr hidden constant [11 x i8] c"9BaseClass\00", align 1
@_ZTI9BaseClass = linkonce_odr hidden constant { i8*, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv117__class_type_infoE, i64 2) to i8*), i8* inttoptr (i64 add (i64 ptrtoint ([11 x i8]* @_ZTS9BaseClass to i64), i64 -9223372036854775808) to i8*) }, align 8
@_ZTI12DerivedClass = linkonce_odr hidden constant { i8*, i8*, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2) to i8*), i8* inttoptr (i64 add (i64 ptrtoint ([15 x i8]* @_ZTS12DerivedClass to i64), i64 -9223372036854775808) to i8*), i8* bitcast ({ i8*, i8* }* @_ZTI9BaseClass to i8*) }, align 8
@_ZTV9BaseClass = linkonce_odr unnamed_addr constant { [5 x i8*] } { [5 x i8*] [i8* null, i8* bitcast ({ i8*, i8* }* @_ZTI9BaseClass to i8*), i8* bitcast (%class.BaseClass* (%class.BaseClass*)* @_ZN9BaseClassD1Ev to i8*), i8* bitcast (void (%class.BaseClass*)* @_ZN9BaseClassD0Ev to i8*), i8* bitcast (i32 (%class.BaseClass*)* @_ZN9BaseClass4FuncEv to i8*)] }, align 8

; Function Attrs: noinline norecurse optnone ssp uwtable
define i32 @main(i32 %0, i8** %1) #0 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 8
  %6 = alloca %class.BaseClass*, align 8
  %7 = alloca i8*, align 8
  %8 = alloca i32, align 4
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4
  store i8** %1, i8*** %5, align 8
  %9 = call noalias nonnull i8* @_Znwm(i64 8) #5
  %10 = bitcast i8* %9 to %class.DerivedClass*
  %11 = invoke %class.DerivedClass* @_ZN12DerivedClassC1Ev(%class.DerivedClass* %10)
          to label %12 unwind label %20

12:                                               ; preds = %2
  %13 = bitcast %class.DerivedClass* %10 to %class.BaseClass*
  store %class.BaseClass* %13, %class.BaseClass** %6, align 8
  %14 = load %class.BaseClass*, %class.BaseClass** %6, align 8
  %15 = bitcast %class.BaseClass* %14 to i32 (%class.BaseClass*)***
  %16 = load i32 (%class.BaseClass*)**, i32 (%class.BaseClass*)*** %15, align 8
  %17 = getelementptr inbounds i32 (%class.BaseClass*)*, i32 (%class.BaseClass*)** %16, i64 2
  %18 = load i32 (%class.BaseClass*)*, i32 (%class.BaseClass*)** %17, align 8
  %19 = call i32 %18(%class.BaseClass* %14)
  ret i32 %19

20:                                               ; preds = %2
  %21 = landingpad { i8*, i32 }
          cleanup
  %22 = extractvalue { i8*, i32 } %21, 0
  store i8* %22, i8** %7, align 8
  %23 = extractvalue { i8*, i32 } %21, 1
  store i32 %23, i32* %8, align 4
  call void @_ZdlPv(i8* %9) #6
  br label %24

24:                                               ; preds = %20
  %25 = load i8*, i8** %7, align 8
  %26 = load i32, i32* %8, align 4
  %27 = insertvalue { i8*, i32 } undef, i8* %25, 0
  %28 = insertvalue { i8*, i32 } %27, i32 %26, 1
  resume { i8*, i32 } %28
}

; Function Attrs: nobuiltin allocsize(0)
declare nonnull i8* @_Znwm(i64) #1

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr %class.DerivedClass* @_ZN12DerivedClassC1Ev(%class.DerivedClass* returned %0) unnamed_addr #2 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  %4 = call %class.DerivedClass* @_ZN12DerivedClassC2Ev(%class.DerivedClass* %3)
  ret %class.DerivedClass* %3
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nobuiltin nounwind
declare void @_ZdlPv(i8*) #3

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr %class.DerivedClass* @_ZN12DerivedClassC2Ev(%class.DerivedClass* returned %0) unnamed_addr #2 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  %4 = bitcast %class.DerivedClass* %3 to %class.BaseClass*
  %5 = call %class.BaseClass* @_ZN9BaseClassC2Ev(%class.BaseClass* %4)
  %6 = bitcast %class.DerivedClass* %3 to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTV12DerivedClass, i32 0, inrange i32 0, i32 2) to i32 (...)**), i32 (...)*** %6, align 8
  ret %class.DerivedClass* %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr %class.BaseClass* @_ZN9BaseClassC2Ev(%class.BaseClass* returned %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  %4 = bitcast %class.BaseClass* %3 to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTV9BaseClass, i32 0, inrange i32 0, i32 2) to i32 (...)**), i32 (...)*** %4, align 8
  ret %class.BaseClass* %3
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr %class.DerivedClass* @_ZN12DerivedClassD1Ev(%class.DerivedClass* returned %0) unnamed_addr #2 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  %4 = call %class.DerivedClass* @_ZN12DerivedClassD2Ev(%class.DerivedClass* %3)
  ret %class.DerivedClass* %3
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN12DerivedClassD0Ev(%class.DerivedClass* %0) unnamed_addr #2 align 2 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %2 = alloca %class.DerivedClass*, align 8
  %3 = alloca i8*, align 8
  %4 = alloca i32, align 4
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %5 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  %6 = invoke %class.DerivedClass* @_ZN12DerivedClassD1Ev(%class.DerivedClass* %5)
          to label %7 unwind label %9

7:                                                ; preds = %1
  %8 = bitcast %class.DerivedClass* %5 to i8*
  call void @_ZdlPv(i8* %8) #6
  ret void

9:                                                ; preds = %1
  %10 = landingpad { i8*, i32 }
          cleanup
  %11 = extractvalue { i8*, i32 } %10, 0
  store i8* %11, i8** %3, align 8
  %12 = extractvalue { i8*, i32 } %10, 1
  store i32 %12, i32* %4, align 4
  %13 = bitcast %class.DerivedClass* %5 to i8*
  call void @_ZdlPv(i8* %13) #6
  br label %14

14:                                               ; preds = %9
  %15 = load i8*, i8** %3, align 8
  %16 = load i32, i32* %4, align 4
  %17 = insertvalue { i8*, i32 } undef, i8* %15, 0
  %18 = insertvalue { i8*, i32 } %17, i32 %16, 1
  resume { i8*, i32 } %18
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i32 @_ZN12DerivedClass4FuncEv(%class.DerivedClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  ret i32 1
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr %class.BaseClass* @_ZN9BaseClassD1Ev(%class.BaseClass* returned %0) unnamed_addr #2 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  %4 = call %class.BaseClass* @_ZN9BaseClassD2Ev(%class.BaseClass* %3)
  ret %class.BaseClass* %3
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN9BaseClassD0Ev(%class.BaseClass* %0) unnamed_addr #2 align 2 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %2 = alloca %class.BaseClass*, align 8
  %3 = alloca i8*, align 8
  %4 = alloca i32, align 4
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %5 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  %6 = invoke %class.BaseClass* @_ZN9BaseClassD1Ev(%class.BaseClass* %5)
          to label %7 unwind label %9

7:                                                ; preds = %1
  %8 = bitcast %class.BaseClass* %5 to i8*
  call void @_ZdlPv(i8* %8) #6
  ret void

9:                                                ; preds = %1
  %10 = landingpad { i8*, i32 }
          cleanup
  %11 = extractvalue { i8*, i32 } %10, 0
  store i8* %11, i8** %3, align 8
  %12 = extractvalue { i8*, i32 } %10, 1
  store i32 %12, i32* %4, align 4
  %13 = bitcast %class.BaseClass* %5 to i8*
  call void @_ZdlPv(i8* %13) #6
  br label %14

14:                                               ; preds = %9
  %15 = load i8*, i8** %3, align 8
  %16 = load i32, i32* %4, align 4
  %17 = insertvalue { i8*, i32 } undef, i8* %15, 0
  %18 = insertvalue { i8*, i32 } %17, i32 %16, 1
  resume { i8*, i32 } %18
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i32 @_ZN9BaseClass4FuncEv(%class.BaseClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  ret i32 0
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr %class.BaseClass* @_ZN9BaseClassD2Ev(%class.BaseClass* returned %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  ret %class.BaseClass* %3
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr %class.DerivedClass* @_ZN12DerivedClassD2Ev(%class.DerivedClass* returned %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  %4 = bitcast %class.DerivedClass* %3 to %class.BaseClass*
  %5 = call %class.BaseClass* @_ZN9BaseClassD2Ev(%class.BaseClass* %4)
  ret %class.DerivedClass* %3
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
!2 = !{i32 1, !"branch-target-enforcement", i32 0}
!3 = !{i32 1, !"sign-return-address", i32 0}
!4 = !{i32 1, !"sign-return-address-all", i32 0}
!5 = !{i32 1, !"sign-return-address-with-bkey", i32 0}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 1}
!8 = !{i32 7, !"frame-pointer", i32 1}
!9 = !{!"Apple clang version 14.0.0 (clang-1400.0.29.202)"}
