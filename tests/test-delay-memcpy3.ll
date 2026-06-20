; RUN: %seadsa  %butd_dsa --sea-dsa-dot %s --sea-dsa-delay-mem-transfer --sea-dsa-stats --sea-dsa-type-aware --sea-dsa-dot-outdir=%T/test-delay-memcpy3.ll
; RUN: %cmp-graphs %tests/test-delay-memcpy3.ll.main.mem.dot %T/test-delay-memcpy3.ll/main.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-delay-memcpy3.ll'
source_filename = "single_node.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.Test = type { %struct.Inner, %struct.Inner }
%struct.Inner = type { %struct.A*, i32* }
%struct.A = type { i32, i32* }

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 !dbg !18 {
  %1 = alloca i32, align 4
  %2 = alloca %struct.Test, align 8
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  call void @llvm.dbg.declare(metadata %struct.Test* %2, metadata !22, metadata !DIExpression()), !dbg !31
  call void @llvm.dbg.declare(metadata i32* %3, metadata !32, metadata !DIExpression()), !dbg !33
  store i32 4, i32* %3, align 4, !dbg !33
  %4 = call noalias i8* @malloc(i64 noundef 16) #5, !dbg !34
  %5 = bitcast i8* %4 to %struct.A*, !dbg !35
  %6 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 0, !dbg !36
  %7 = getelementptr inbounds %struct.Inner, %struct.Inner* %6, i32 0, i32 0, !dbg !37
  store %struct.A* %5, %struct.A** %7, align 8, !dbg !38
  %8 = call noalias i8* @malloc(i64 noundef 4) #5, !dbg !39
  %9 = bitcast i8* %8 to i32*, !dbg !40
  %10 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 0, !dbg !41
  %11 = getelementptr inbounds %struct.Inner, %struct.Inner* %10, i32 0, i32 1, !dbg !42
  store i32* %9, i32** %11, align 8, !dbg !43
  %12 = load i32, i32* %3, align 4, !dbg !44
  %13 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 0, !dbg !45
  %14 = getelementptr inbounds %struct.Inner, %struct.Inner* %13, i32 0, i32 1, !dbg !46
  %15 = load i32*, i32** %14, align 8, !dbg !46
  store i32 %12, i32* %15, align 4, !dbg !47
  %16 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 0, !dbg !48
  %17 = getelementptr inbounds %struct.Inner, %struct.Inner* %16, i32 0, i32 0, !dbg !49
  %18 = load %struct.A*, %struct.A** %17, align 8, !dbg !49
  %19 = getelementptr inbounds %struct.A, %struct.A* %18, i32 0, i32 0, !dbg !50
  store i32 5, i32* %19, align 8, !dbg !51
  %20 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 0, !dbg !52
  %21 = getelementptr inbounds %struct.Inner, %struct.Inner* %20, i32 0, i32 0, !dbg !53
  %22 = load %struct.A*, %struct.A** %21, align 8, !dbg !53
  %23 = getelementptr inbounds %struct.A, %struct.A* %22, i32 0, i32 1, !dbg !54
  store i32* %3, i32** %23, align 8, !dbg !55
  %24 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 1, !dbg !56
  %25 = bitcast %struct.Inner* %24 to i8*, !dbg !57
  %26 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 0, !dbg !58
  %27 = bitcast %struct.Inner* %26 to i8*, !dbg !57
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %25, i8* align 8 %27, i64 16, i1 false), !dbg !57
  %28 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 0, !dbg !59
  %29 = bitcast %struct.Inner* %28 to i8*, !dbg !60
  call void @sea_dsa_set_read(i8* noundef %29), !dbg !61
  %30 = getelementptr inbounds %struct.Test, %struct.Test* %2, i32 0, i32 1, !dbg !62
  %31 = bitcast %struct.Inner* %30 to i8*, !dbg !63
  call void @sea_dsa_set_modified(i8* noundef %31), !dbg !64
  ret i32 0, !dbg !65
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind
declare noalias i8* @malloc(i64 noundef) #2

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #3

declare void @sea_dsa_set_read(i8* noundef) #4

declare void @sea_dsa_set_modified(i8* noundef) #4

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #2 = { nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { argmemonly nofree nounwind willreturn }
attributes #4 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!10, !11, !12, !13, !14, !15, !16}
!llvm.ident = !{!17}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "single_node.c", directory: "/home/ljgy/Works/seatools/sea-dsa/build/c_emp", checksumkind: CSK_MD5, checksum: "76fac8b6d63a6b05e574362f498ca9f6")
!2 = !{!3, !9}
!3 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !4, size: 64)
!4 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "A", file: !1, line: 11, size: 128, elements: !5)
!5 = !{!6, !8}
!6 = !DIDerivedType(tag: DW_TAG_member, name: "x", scope: !4, file: !1, line: 12, baseType: !7, size: 32)
!7 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!8 = !DIDerivedType(tag: DW_TAG_member, name: "y", scope: !4, file: !1, line: 13, baseType: !9, size: 64, offset: 64)
!9 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !7, size: 64)
!10 = !{i32 7, !"Dwarf Version", i32 5}
!11 = !{i32 2, !"Debug Info Version", i32 3}
!12 = !{i32 1, !"wchar_size", i32 4}
!13 = !{i32 7, !"PIC Level", i32 2}
!14 = !{i32 7, !"PIE Level", i32 2}
!15 = !{i32 7, !"uwtable", i32 1}
!16 = !{i32 7, !"frame-pointer", i32 2}
!17 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
!18 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 33, type: !19, scopeLine: 33, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !21)
!19 = !DISubroutineType(types: !20)
!20 = !{!7}
!21 = !{}
!22 = !DILocalVariable(name: "a", scope: !18, file: !1, line: 34, type: !23)
!23 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Test", file: !1, line: 28, size: 256, elements: !24)
!24 = !{!25, !30}
!25 = !DIDerivedType(tag: DW_TAG_member, name: "first", scope: !23, file: !1, line: 29, baseType: !26, size: 128)
!26 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Inner", file: !1, line: 22, size: 128, elements: !27)
!27 = !{!28, !29}
!28 = !DIDerivedType(tag: DW_TAG_member, name: "x", scope: !26, file: !1, line: 23, baseType: !3, size: 64)
!29 = !DIDerivedType(tag: DW_TAG_member, name: "y", scope: !26, file: !1, line: 24, baseType: !9, size: 64, offset: 64)
!30 = !DIDerivedType(tag: DW_TAG_member, name: "second", scope: !23, file: !1, line: 30, baseType: !26, size: 128, offset: 128)
!31 = !DILocation(line: 34, column: 15, scope: !18)
!32 = !DILocalVariable(name: "x", scope: !18, file: !1, line: 35, type: !7)
!33 = !DILocation(line: 35, column: 7, scope: !18)
!34 = !DILocation(line: 38, column: 27, scope: !18)
!35 = !DILocation(line: 38, column: 15, scope: !18)
!36 = !DILocation(line: 38, column: 5, scope: !18)
!37 = !DILocation(line: 38, column: 11, scope: !18)
!38 = !DILocation(line: 38, column: 13, scope: !18)
!39 = !DILocation(line: 39, column: 22, scope: !18)
!40 = !DILocation(line: 39, column: 15, scope: !18)
!41 = !DILocation(line: 39, column: 5, scope: !18)
!42 = !DILocation(line: 39, column: 11, scope: !18)
!43 = !DILocation(line: 39, column: 13, scope: !18)
!44 = !DILocation(line: 40, column: 18, scope: !18)
!45 = !DILocation(line: 40, column: 7, scope: !18)
!46 = !DILocation(line: 40, column: 13, scope: !18)
!47 = !DILocation(line: 40, column: 16, scope: !18)
!48 = !DILocation(line: 41, column: 5, scope: !18)
!49 = !DILocation(line: 41, column: 11, scope: !18)
!50 = !DILocation(line: 41, column: 14, scope: !18)
!51 = !DILocation(line: 41, column: 16, scope: !18)
!52 = !DILocation(line: 42, column: 5, scope: !18)
!53 = !DILocation(line: 42, column: 11, scope: !18)
!54 = !DILocation(line: 42, column: 14, scope: !18)
!55 = !DILocation(line: 42, column: 16, scope: !18)
!56 = !DILocation(line: 48, column: 13, scope: !18)
!57 = !DILocation(line: 48, column: 3, scope: !18)
!58 = !DILocation(line: 48, column: 24, scope: !18)
!59 = !DILocation(line: 51, column: 23, scope: !18)
!60 = !DILocation(line: 51, column: 20, scope: !18)
!61 = !DILocation(line: 51, column: 3, scope: !18)
!62 = !DILocation(line: 52, column: 27, scope: !18)
!63 = !DILocation(line: 52, column: 24, scope: !18)
!64 = !DILocation(line: 52, column: 3, scope: !18)
!65 = !DILocation(line: 54, column: 3, scope: !18)
