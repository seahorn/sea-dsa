; RUN: %seadsa  %butd_dsa --sea-dsa-dot %s --sea-dsa-lazy-mem-transfer --sea-dsa-stats --sea-dsa-type-aware --sea-dsa-dot-outdir=%T/test-delay-memcpy1.ll
; RUN: %cmp-graphs %tests/test-delay-memcpy1.ll.main.mem.dot %T/test-delay-memcpy1.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-delay-memcpy1.ll'
source_filename = "llvm-link"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.A = type { i32, %struct.context, i32, i32 }
%struct.context = type { i32 (i8*, i32*)*, i8* }
%struct.B = type { %struct.A, i32, i32, %struct.context }

; Function Attrs: nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #0 !dbg !43 {
  %1 = alloca i64, align 8
  %2 = alloca i32, align 4
  %malloc2 = alloca %struct.A, align 4
  call void @llvm.dbg.value(metadata %struct.A* %malloc2, metadata !49, metadata !DIExpression()), !dbg !76
  %3 = bitcast i32* %2 to i8*, !dbg !77
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %3) #6, !dbg !77
  %4 = call i32 @nd_int() #7, !dbg !77
  call void @llvm.dbg.value(metadata i32 %4, metadata !66, metadata !DIExpression()), !dbg !78
  store i32 %4, i32* %2, align 4, !dbg !77, !tbaa !79
  call void @llvm.dbg.value(metadata i32* %2, metadata !66, metadata !DIExpression(DW_OP_deref)), !dbg !78
  call void @__CRAB_intrinsic_add_tag(i8* noundef nonnull %3, i64 noundef 1) #8, !dbg !77
  %5 = load i32, i32* %2, align 4, !dbg !77, !tbaa !79
  call void @llvm.dbg.value(metadata i32 %5, metadata !66, metadata !DIExpression()), !dbg !78
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %3) #6, !dbg !83
  %6 = getelementptr inbounds %struct.A, %struct.A* %malloc2, i64 0, i32 0, !dbg !84
  store i32 %5, i32* %6, align 8, !dbg !85, !tbaa !86
  %7 = getelementptr inbounds %struct.A, %struct.A* %malloc2, i64 0, i32 2, !dbg !90
  store i32 4, i32* %7, align 8, !dbg !91, !tbaa !92
  %8 = getelementptr inbounds %struct.A, %struct.A* %malloc2, i64 0, i32 3, !dbg !93
  store i32 7, i32* %8, align 4, !dbg !94, !tbaa !95
  %9 = getelementptr inbounds %struct.A, %struct.A* %malloc2, i64 0, i32 1, i32 0, !dbg !96
  store i32 (i8*, i32*)* null, i32 (i8*, i32*)** %9, align 8, !dbg !97, !tbaa !98
  %10 = getelementptr inbounds %struct.A, %struct.A* %malloc2, i64 0, i32 1, i32 1, !dbg !99
  store i8* null, i8** %10, align 8, !dbg !100, !tbaa !101
  %malloc13 = alloca %struct.B, align 4
  call void @llvm.dbg.value(metadata %struct.B* %malloc13, metadata !68, metadata !DIExpression()), !dbg !76
  %11 = bitcast %struct.B* %malloc13 to i8*, !dbg !102
  %12 = bitcast %struct.A* %malloc2 to i8*, !dbg !102
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* noundef nonnull align 8 dereferenceable(32) %11, i8* noundef nonnull align 8 dereferenceable(32) %12, i64 32, i1 false), !dbg !102, !tbaa.struct !103
  %13 = getelementptr inbounds %struct.B, %struct.B* %malloc13, i64 0, i32 0, i32 0, !dbg !105
  store i32 7, i32* %13, align 8, !dbg !106, !tbaa !107
  %14 = getelementptr inbounds %struct.B, %struct.B* %malloc13, i64 0, i32 1, !dbg !109
  store i32 5, i32* %14, align 8, !dbg !110, !tbaa !111
  %15 = getelementptr inbounds %struct.B, %struct.B* %malloc13, i64 0, i32 2, !dbg !112
  store i32 6, i32* %15, align 4, !dbg !113, !tbaa !114
  %16 = getelementptr inbounds %struct.B, %struct.B* %malloc13, i64 0, i32 3, !dbg !115
  %17 = bitcast %struct.context* %16 to i8*, !dbg !116
  %18 = getelementptr inbounds %struct.A, %struct.A* %malloc2, i64 0, i32 1, !dbg !117
  %19 = bitcast %struct.context* %18 to i8*, !dbg !118
  %20 = bitcast i64* %1 to i8*, !dbg !119
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %20), !dbg !119
  call void @llvm.dbg.value(metadata i8* %17, metadata !125, metadata !DIExpression()) #6, !dbg !119
  call void @llvm.dbg.value(metadata i8* %19, metadata !126, metadata !DIExpression()) #6, !dbg !119
  call void @llvm.dbg.value(metadata i64 16, metadata !127, metadata !DIExpression()) #6, !dbg !119
  store i64 16, i64* %1, align 8, !tbaa !131
  call void @__CRAB_intrinsic_move_tags(i8* noundef nonnull %19, i8* noundef nonnull %17) #8, !dbg !133
  call void @llvm.dbg.value(metadata i8* %20, metadata !128, metadata !DIExpression()) #6, !dbg !134
  call void @llvm.dbg.value(metadata i64* %1, metadata !127, metadata !DIExpression(DW_OP_deref)) #6, !dbg !119
  call void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef nonnull %20, i64 noundef 1) #8, !dbg !135
  %21 = load i64, i64* %1, align 8, !dbg !136, !tbaa !131
  call void @llvm.dbg.value(metadata i64 %21, metadata !127, metadata !DIExpression()) #6, !dbg !119
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 4 %17, i8* nonnull align 4 %19, i64 %21, i1 false) #6, !dbg !137
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %20), !dbg !138
  %22 = bitcast i32* %14 to i8*, !dbg !139
  call void @llvm.dbg.value(metadata i8* %22, metadata !140, metadata !DIExpression()) #6, !dbg !146
  call void @__CRAB_intrinsic_check_has_tag(i8* noundef nonnull %22, i64 noundef 1) #8, !dbg !148
  ret i32 0, !dbg !149
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nounwind
declare !dbg !150 i32 @nd_int() local_unnamed_addr #2

declare void @__CRAB_intrinsic_add_tag(i8* noundef, i64 noundef) local_unnamed_addr #3

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #4

declare void @__CRAB_intrinsic_move_tags(i8* noundef, i8* noundef) local_unnamed_addr #3

declare void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef, i64 noundef) local_unnamed_addr #3

declare void @__CRAB_intrinsic_check_has_tag(i8* noundef, i64 noundef) local_unnamed_addr #3

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #5

attributes #0 = { nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { argmemonly nounwind "frame-pointer"="none" "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { "frame-pointer"="none" "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { argmemonly nofree nounwind willreturn }
attributes #5 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #6 = { nounwind }
attributes #7 = { argmemonly nounwind "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" }
attributes #8 = { nounwind "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" }

!llvm.dbg.cu = !{!0, !4, !24, !26, !31}
!llvm.ident = !{!36, !36, !36, !36, !36}
!llvm.module.flags = !{!37, !38, !39, !40, !41, !42}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/benchmarks/field/field7/field7.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/benchmarks/field/field7", checksumkind: CSK_MD5, checksum: "bd30a3181c54dab9aea5aec10068ceb4")
!2 = !{!3}
!3 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!4 = distinct !DICompileUnit(language: DW_LANG_C99, file: !5, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !6, splitDebugInlining: false, nameTableKind: None)
!5 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/lib/nd_clam.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/lib", checksumkind: CSK_MD5, checksum: "a3e9bf8d46f9e3f9034b51583e568cfb")
!6 = !{!7, !10, !15, !18, !21, !23, !14}
!7 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !8, line: 46, baseType: !9)
!8 = !DIFile(filename: "/usr/lib/llvm-14/lib/clang/14.0.0/include/stddef.h", directory: "", checksumkind: CSK_MD5, checksum: "2499dd2361b915724b073282bea3a7bc")
!9 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!10 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint8_t", file: !11, line: 24, baseType: !12)
!11 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/stdint-uintn.h", directory: "", checksumkind: CSK_MD5, checksum: "2bf2ae53c58c01b1a1b9383b5195125c")
!12 = !DIDerivedType(tag: DW_TAG_typedef, name: "__uint8_t", file: !13, line: 38, baseType: !14)
!13 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types.h", directory: "", checksumkind: CSK_MD5, checksum: "d108b5f93a74c50510d7d9bc0ab36df9")
!14 = !DIBasicType(name: "unsigned char", size: 8, encoding: DW_ATE_unsigned_char)
!15 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint16_t", file: !11, line: 25, baseType: !16)
!16 = !DIDerivedType(tag: DW_TAG_typedef, name: "__uint16_t", file: !13, line: 40, baseType: !17)
!17 = !DIBasicType(name: "unsigned short", size: 16, encoding: DW_ATE_unsigned)
!18 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint32_t", file: !11, line: 26, baseType: !19)
!19 = !DIDerivedType(tag: DW_TAG_typedef, name: "__uint32_t", file: !13, line: 42, baseType: !20)
!20 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!21 = !DIDerivedType(tag: DW_TAG_typedef, name: "uint64_t", file: !11, line: 27, baseType: !22)
!22 = !DIDerivedType(tag: DW_TAG_typedef, name: "__uint64_t", file: !13, line: 45, baseType: !9)
!23 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!24 = distinct !DICompileUnit(language: DW_LANG_C99, file: !25, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !2, splitDebugInlining: false, nameTableKind: None)
!25 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/lib/sea_allocators.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/lib", checksumkind: CSK_MD5, checksum: "195a095d3b3f671498f7ab8158e09f3f")
!26 = distinct !DICompileUnit(language: DW_LANG_C99, file: !27, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !28, splitDebugInlining: false, nameTableKind: None)
!27 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/lib/clam_taint.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/lib", checksumkind: CSK_MD5, checksum: "b6c78d8c3355cb659d9cac7f416aa168")
!28 = !{!29, !30}
!29 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !23, size: 64)
!30 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!31 = distinct !DICompileUnit(language: DW_LANG_C99, file: !32, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !33, splitDebugInlining: false, nameTableKind: None)
!32 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/lib/lib_func.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/lib", checksumkind: CSK_MD5, checksum: "889ddfbf93ec8cf966ffe2347537d028")
!33 = !{!34}
!34 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !35, size: 64)
!35 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!36 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
!37 = !{i32 7, !"Dwarf Version", i32 5}
!38 = !{i32 2, !"Debug Info Version", i32 3}
!39 = !{i32 1, !"wchar_size", i32 4}
!40 = !{i32 7, !"PIC Level", i32 2}
!41 = !{i32 7, !"PIE Level", i32 2}
!42 = !{i32 7, !"uwtable", i32 1}
!43 = distinct !DISubprogram(name: "main", scope: !44, file: !44, line: 62, type: !45, scopeLine: 62, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !48)
!44 = !DIFile(filename: "benchmarks/field/field7/field7.c", directory: "/home/ljgy/Works/seatools/taint-benchs", checksumkind: CSK_MD5, checksum: "bd30a3181c54dab9aea5aec10068ceb4")
!45 = !DISubroutineType(types: !46)
!46 = !{!47}
!47 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!48 = !{!49, !66, !68}
!49 = !DILocalVariable(name: "a", scope: !43, file: !44, line: 64, type: !50)
!50 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !51, size: 64)
!51 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "A", file: !44, line: 20, size: 256, elements: !52)
!52 = !{!53, !54, !64, !65}
!53 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !51, file: !44, line: 21, baseType: !47, size: 32)
!54 = !DIDerivedType(tag: DW_TAG_member, name: "c", scope: !51, file: !44, line: 22, baseType: !55, size: 128, offset: 64)
!55 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "context", file: !44, line: 15, size: 128, elements: !56)
!56 = !{!57, !63}
!57 = !DIDerivedType(tag: DW_TAG_member, name: "notify", scope: !55, file: !44, line: 16, baseType: !58, size: 64)
!58 = !DIDerivedType(tag: DW_TAG_typedef, name: "call_back_t", file: !44, line: 13, baseType: !59)
!59 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !60, size: 64)
!60 = !DISubroutineType(types: !61)
!61 = !{!47, !3, !62}
!62 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !47, size: 64)
!63 = !DIDerivedType(tag: DW_TAG_member, name: "context", scope: !55, file: !44, line: 17, baseType: !3, size: 64, offset: 64)
!64 = !DIDerivedType(tag: DW_TAG_member, name: "b", scope: !51, file: !44, line: 23, baseType: !47, size: 32, offset: 192)
!65 = !DIDerivedType(tag: DW_TAG_member, name: "d", scope: !51, file: !44, line: 24, baseType: !47, size: 32, offset: 224)
!66 = !DILocalVariable(name: "x", scope: !67, file: !44, line: 66, type: !47)
!67 = distinct !DILexicalBlock(scope: !43, file: !44, line: 66, column: 10)
!68 = !DILocalVariable(name: "b", scope: !43, file: !44, line: 76, type: !69)
!69 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !70, size: 64)
!70 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "B", file: !44, line: 27, size: 448, elements: !71)
!71 = !{!72, !73, !74, !75}
!72 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !70, file: !44, line: 28, baseType: !51, size: 256)
!73 = !DIDerivedType(tag: DW_TAG_member, name: "x", scope: !70, file: !44, line: 29, baseType: !47, size: 32, offset: 256)
!74 = !DIDerivedType(tag: DW_TAG_member, name: "y", scope: !70, file: !44, line: 30, baseType: !47, size: 32, offset: 288)
!75 = !DIDerivedType(tag: DW_TAG_member, name: "c2", scope: !70, file: !44, line: 31, baseType: !55, size: 128, offset: 320)
!76 = !DILocation(line: 0, scope: !43)
!77 = !DILocation(line: 66, column: 10, scope: !67)
!78 = !DILocation(line: 0, scope: !67)
!79 = !{!80, !80, i64 0}
!80 = !{!"int", !81, i64 0}
!81 = !{!"omnipotent char", !82, i64 0}
!82 = !{!"Simple C/C++ TBAA"}
!83 = !DILocation(line: 66, column: 10, scope: !43)
!84 = !DILocation(line: 66, column: 6, scope: !43)
!85 = !DILocation(line: 66, column: 8, scope: !43)
!86 = !{!87, !80, i64 0}
!87 = !{!"A", !80, i64 0, !88, i64 8, !80, i64 24, !80, i64 28}
!88 = !{!"context", !89, i64 0, !89, i64 8}
!89 = !{!"any pointer", !81, i64 0}
!90 = !DILocation(line: 67, column: 6, scope: !43)
!91 = !DILocation(line: 67, column: 8, scope: !43)
!92 = !{!87, !80, i64 24}
!93 = !DILocation(line: 68, column: 6, scope: !43)
!94 = !DILocation(line: 68, column: 8, scope: !43)
!95 = !{!87, !80, i64 28}
!96 = !DILocation(line: 74, column: 8, scope: !43)
!97 = !DILocation(line: 74, column: 15, scope: !43)
!98 = !{!87, !89, i64 8}
!99 = !DILocation(line: 75, column: 8, scope: !43)
!100 = !DILocation(line: 75, column: 16, scope: !43)
!101 = !{!87, !89, i64 16}
!102 = !DILocation(line: 78, column: 10, scope: !43)
!103 = !{i64 0, i64 4, !79, i64 8, i64 8, !104, i64 16, i64 8, !104, i64 24, i64 4, !79, i64 28, i64 4, !79}
!104 = !{!89, !89, i64 0}
!105 = !DILocation(line: 79, column: 8, scope: !43)
!106 = !DILocation(line: 79, column: 10, scope: !43)
!107 = !{!108, !80, i64 0}
!108 = !{!"B", !87, i64 0, !80, i64 32, !80, i64 36, !88, i64 40}
!109 = !DILocation(line: 80, column: 6, scope: !43)
!110 = !DILocation(line: 80, column: 8, scope: !43)
!111 = !{!108, !80, i64 32}
!112 = !DILocation(line: 81, column: 6, scope: !43)
!113 = !DILocation(line: 81, column: 8, scope: !43)
!114 = !{!108, !80, i64 36}
!115 = !DILocation(line: 82, column: 14, scope: !43)
!116 = !DILocation(line: 82, column: 10, scope: !43)
!117 = !DILocation(line: 82, column: 22, scope: !43)
!118 = !DILocation(line: 82, column: 18, scope: !43)
!119 = !DILocation(line: 0, scope: !120, inlinedAt: !130)
!120 = distinct !DISubprogram(name: "memcpy", scope: !121, file: !121, line: 31, type: !122, scopeLine: 31, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !31, retainedNodes: !124)
!121 = !DIFile(filename: "lib/lib_func.c", directory: "/home/ljgy/Works/seatools/taint-benchs", checksumkind: CSK_MD5, checksum: "889ddfbf93ec8cf966ffe2347537d028")
!122 = !DISubroutineType(types: !123)
!123 = !{!3, !3, !34, !7}
!124 = !{!125, !126, !127, !128}
!125 = !DILocalVariable(name: "dst", arg: 1, scope: !120, file: !121, line: 31, type: !3)
!126 = !DILocalVariable(name: "src", arg: 2, scope: !120, file: !121, line: 31, type: !34)
!127 = !DILocalVariable(name: "len", arg: 3, scope: !120, file: !121, line: 31, type: !7)
!128 = !DILocalVariable(name: "__sea_is_not_tainted_ptr", scope: !129, file: !121, line: 34, type: !34)
!129 = distinct !DILexicalBlock(scope: !120, file: !121, line: 34, column: 3)
!130 = distinct !DILocation(line: 82, column: 3, scope: !43)
!131 = !{!132, !132, i64 0}
!132 = !{!"long", !81, i64 0}
!133 = !DILocation(line: 33, column: 3, scope: !120, inlinedAt: !130)
!134 = !DILocation(line: 0, scope: !129, inlinedAt: !130)
!135 = !DILocation(line: 34, column: 3, scope: !129, inlinedAt: !130)
!136 = !DILocation(line: 38, column: 37, scope: !120, inlinedAt: !130)
!137 = !DILocation(line: 38, column: 10, scope: !120, inlinedAt: !130)
!138 = !DILocation(line: 38, column: 3, scope: !120, inlinedAt: !130)
!139 = !DILocation(line: 83, column: 14, scope: !43)
!140 = !DILocalVariable(name: "ptr", arg: 1, scope: !141, file: !142, line: 63, type: !34)
!141 = distinct !DISubprogram(name: "is_tainted", scope: !142, file: !142, line: 63, type: !143, scopeLine: 63, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !26, retainedNodes: !145)
!142 = !DIFile(filename: "lib/clam_taint.c", directory: "/home/ljgy/Works/seatools/taint-benchs", checksumkind: CSK_MD5, checksum: "b6c78d8c3355cb659d9cac7f416aa168")
!143 = !DISubroutineType(types: !144)
!144 = !{null, !34}
!145 = !{!140}
!146 = !DILocation(line: 0, scope: !141, inlinedAt: !147)
!147 = distinct !DILocation(line: 83, column: 3, scope: !43)
!148 = !DILocation(line: 65, column: 3, scope: !141, inlinedAt: !147)
!149 = !DILocation(line: 85, column: 3, scope: !43)
!150 = !DISubprogram(name: "nd_int", scope: !151, file: !151, line: 26, type: !45, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !152)
!151 = !DIFile(filename: "include/nondet.h", directory: "/home/ljgy/Works/seatools/taint-benchs", checksumkind: CSK_MD5, checksum: "104d96909c6a13b82d2807f6f8eae503")
!152 = !{}
