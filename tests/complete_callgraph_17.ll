; RUN: %seadsa %s  --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_17.ll
; RUN: %cmp-graphs %tests/complete_callgraph_17.dot %T/complete_callgraph_17.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$


; ModuleID = 'complete_callgraph_17.bc'
source_filename = "complete_callgraph_17.cpp"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.15.0"

%class.BaseClass = type { i32 (...)** }
%class.DerivedClass = type { %class.BaseClass }

@_ZTV12DerivedClass = linkonce_odr unnamed_addr constant { [5 x i8*] } { [5 x i8*] [i8* null, i8* bitcast ({ i8*, i8*, i8* }* @_ZTI12DerivedClass to i8*), i8* bitcast (void (%class.DerivedClass*)* @_ZN12DerivedClassD1Ev to i8*), i8* bitcast (void (%class.DerivedClass*)* @_ZN12DerivedClassD0Ev to i8*), i8* bitcast (i32 (%class.DerivedClass*)* @_ZN12DerivedClass4FuncEv to i8*)] }, align 8
@_ZTVN10__cxxabiv120__si_class_type_infoE = external global i8*
@_ZTS12DerivedClass = linkonce_odr constant [15 x i8] c"12DerivedClass\00", align 1
@_ZTVN10__cxxabiv117__class_type_infoE = external global i8*
@_ZTS9BaseClass = linkonce_odr constant [11 x i8] c"9BaseClass\00", align 1
@_ZTI9BaseClass = linkonce_odr constant { i8*, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv117__class_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([11 x i8], [11 x i8]* @_ZTS9BaseClass, i32 0, i32 0) }, align 8
@_ZTI12DerivedClass = linkonce_odr constant { i8*, i8*, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([15 x i8], [15 x i8]* @_ZTS12DerivedClass, i32 0, i32 0), i8* bitcast ({ i8*, i8* }* @_ZTI9BaseClass to i8*) }, align 8
@_ZTV9BaseClass = linkonce_odr unnamed_addr constant { [5 x i8*] } { [5 x i8*] [i8* null, i8* bitcast ({ i8*, i8* }* @_ZTI9BaseClass to i8*), i8* bitcast (void (%class.BaseClass*)* @_ZN9BaseClassD1Ev to i8*), i8* bitcast (void (%class.BaseClass*)* @_ZN9BaseClassD0Ev to i8*), i8* bitcast (i32 (%class.BaseClass*)* @_ZN9BaseClass4FuncEv to i8*)] }, align 8

; Function Attrs: noinline norecurse optnone ssp uwtable
define i32 @main(i32 %0, i8** %1) #0 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 8
  %6 = alloca %class.BaseClass*, align 8
  %7 = alloca i8*
  %8 = alloca i32
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4
  store i8** %1, i8*** %5, align 8
  %9 = call i8* @_Znwm(i64 8) #5
  %10 = bitcast i8* %9 to %class.DerivedClass*
  invoke void @_ZN12DerivedClassC1Ev(%class.DerivedClass* %10)
          to label %11 unwind label %19

11:                                               ; preds = %2
  %12 = bitcast %class.DerivedClass* %10 to %class.BaseClass*
  store %class.BaseClass* %12, %class.BaseClass** %6, align 8
  %13 = load %class.BaseClass*, %class.BaseClass** %6, align 8
  %14 = bitcast %class.BaseClass* %13 to i32 (%class.BaseClass*)***
  %15 = load i32 (%class.BaseClass*)**, i32 (%class.BaseClass*)*** %14, align 8
  %16 = getelementptr inbounds i32 (%class.BaseClass*)*, i32 (%class.BaseClass*)** %15, i64 2
  %17 = load i32 (%class.BaseClass*)*, i32 (%class.BaseClass*)** %16, align 8
  %18 = call i32 %17(%class.BaseClass* %13)
  ret i32 %18

19:                                               ; preds = %2
  %20 = landingpad { i8*, i32 }
          cleanup
  %21 = extractvalue { i8*, i32 } %20, 0
  store i8* %21, i8** %7, align 8
  %22 = extractvalue { i8*, i32 } %20, 1
  store i32 %22, i32* %8, align 4
  call void @_ZdlPv(i8* %9) #6
  br label %23

23:                                               ; preds = %19
  %24 = load i8*, i8** %7, align 8
  %25 = load i32, i32* %8, align 4
  %26 = insertvalue { i8*, i32 } undef, i8* %24, 0
  %27 = insertvalue { i8*, i32 } %26, i32 %25, 1
  resume { i8*, i32 } %27
}

; Function Attrs: nobuiltin
declare noalias i8* @_Znwm(i64) #1

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN12DerivedClassC1Ev(%class.DerivedClass* %0) unnamed_addr #2 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  call void @_ZN12DerivedClassC2Ev(%class.DerivedClass* %3)
  ret void
}

declare i32 @__gxx_personality_v0(...)

; Function Attrs: nobuiltin nounwind
declare void @_ZdlPv(i8*) #3

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN12DerivedClassC2Ev(%class.DerivedClass* %0) unnamed_addr #2 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  %4 = bitcast %class.DerivedClass* %3 to %class.BaseClass*
  call void @_ZN9BaseClassC2Ev(%class.BaseClass* %4)
  %5 = bitcast %class.DerivedClass* %3 to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTV12DerivedClass, i32 0, inrange i32 0, i32 2) to i32 (...)**), i32 (...)*** %5, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN9BaseClassC2Ev(%class.BaseClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  %4 = bitcast %class.BaseClass* %3 to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [5 x i8*] }, { [5 x i8*] }* @_ZTV9BaseClass, i32 0, inrange i32 0, i32 2) to i32 (...)**), i32 (...)*** %4, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN12DerivedClassD1Ev(%class.DerivedClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  call void @_ZN12DerivedClassD2Ev(%class.DerivedClass* %3) #7
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN12DerivedClassD0Ev(%class.DerivedClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  call void @_ZN12DerivedClassD1Ev(%class.DerivedClass* %3) #7
  %4 = bitcast %class.DerivedClass* %3 to i8*
  call void @_ZdlPv(i8* %4) #6
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i32 @_ZN12DerivedClass4FuncEv(%class.DerivedClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  ret i32 1
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN9BaseClassD1Ev(%class.BaseClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  call void @_ZN9BaseClassD2Ev(%class.BaseClass* %3) #7
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN9BaseClassD0Ev(%class.BaseClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  call void @_ZN9BaseClassD1Ev(%class.BaseClass* %3) #7
  %4 = bitcast %class.BaseClass* %3 to i8*
  call void @_ZdlPv(i8* %4) #6
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i32 @_ZN9BaseClass4FuncEv(%class.BaseClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  ret i32 0
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN9BaseClassD2Ev(%class.BaseClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.BaseClass*, align 8
  store %class.BaseClass* %0, %class.BaseClass** %2, align 8
  %3 = load %class.BaseClass*, %class.BaseClass** %2, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN12DerivedClassD2Ev(%class.DerivedClass* %0) unnamed_addr #4 align 2 {
  %2 = alloca %class.DerivedClass*, align 8
  store %class.DerivedClass* %0, %class.DerivedClass** %2, align 8
  %3 = load %class.DerivedClass*, %class.DerivedClass** %2, align 8
  %4 = bitcast %class.DerivedClass* %3 to %class.BaseClass*
  call void @_ZN9BaseClassD2Ev(%class.BaseClass* %4) #7
  ret void
}

attributes #0 = { noinline norecurse optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nobuiltin "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { noinline optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nobuiltin nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { noinline nounwind optnone ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { builtin }
attributes #6 = { builtin nounwind }
attributes #7 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 10.0.0 "}
