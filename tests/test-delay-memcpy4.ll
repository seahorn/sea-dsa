; RUN: %seadsa  %butd_dsa --sea-dsa-dot %s --sea-dsa-delay-mem-transfer --sea-dsa-stats --sea-dsa-type-aware --sea-dsa-dot-outdir=%T/test-delay-memcpy4.ll
; RUN: %cmp-graphs %tests/test-delay-memcpy4.ll.main.mem.dot %T/test-delay-memcpy4.ll/main.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; A memcpy with a non-constant length over raw i8/char* buffers. The pointee
; type is i8 (store size 1), but the buffers hold pointers at offsets 0, 8, 16.
; copySize must not shrink to the i8 store size (1); it must copy all three
; links from source to destination (copySize = max(node size, type size) = 24).

; ModuleID = 'test-delay-memcpy4.ll'
source_filename = "build/c_emp/byte_buf.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 !dbg !17 {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8*, align 8
  %6 = alloca i8*, align 8
  %7 = alloca i64, align 8
  %8 = alloca i32*, align 8
  %9 = alloca i32*, align 8
  %10 = alloca i32*, align 8
  store i32 0, i32* %1, align 4
  call void @llvm.dbg.declare(metadata i32* %2, metadata !21, metadata !DIExpression()), !dbg !22
  call void @llvm.dbg.declare(metadata i32* %3, metadata !23, metadata !DIExpression()), !dbg !24
  call void @llvm.dbg.declare(metadata i32* %4, metadata !25, metadata !DIExpression()), !dbg !26
  call void @llvm.dbg.declare(metadata i8** %5, metadata !27, metadata !DIExpression()), !dbg !28
  %11 = call noalias i8* @malloc(i64 noundef 24) #5, !dbg !29
  store i8* %11, i8** %5, align 8, !dbg !28
  call void @llvm.dbg.declare(metadata i8** %6, metadata !30, metadata !DIExpression()), !dbg !31
  %12 = call noalias i8* @malloc(i64 noundef 24) #5, !dbg !32
  store i8* %12, i8** %6, align 8, !dbg !31
  %13 = load i8*, i8** %5, align 8, !dbg !33
  %14 = getelementptr inbounds i8, i8* %13, i64 0, !dbg !34
  %15 = bitcast i8* %14 to i32**, !dbg !35
  store i32* %2, i32** %15, align 8, !dbg !36
  %16 = load i8*, i8** %5, align 8, !dbg !37
  %17 = getelementptr inbounds i8, i8* %16, i64 8, !dbg !38
  %18 = bitcast i8* %17 to i32**, !dbg !39
  store i32* %3, i32** %18, align 8, !dbg !40
  %19 = load i8*, i8** %5, align 8, !dbg !41
  %20 = getelementptr inbounds i8, i8* %19, i64 16, !dbg !42
  %21 = bitcast i8* %20 to i32**, !dbg !43
  store i32* %4, i32** %21, align 8, !dbg !44
  call void @llvm.dbg.declare(metadata i64* %7, metadata !45, metadata !DIExpression()), !dbg !49
  %22 = call i64 @get_len(), !dbg !50
  store i64 %22, i64* %7, align 8, !dbg !49
  %23 = load i8*, i8** %6, align 8, !dbg !51
  %24 = load i8*, i8** %5, align 8, !dbg !52
  %25 = load i64, i64* %7, align 8, !dbg !53
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 1 %23, i8* align 1 %24, i64 %25, i1 false), !dbg !54
  call void @llvm.dbg.declare(metadata i32** %8, metadata !55, metadata !DIExpression()), !dbg !56
  %26 = load i8*, i8** %6, align 8, !dbg !57
  %27 = getelementptr inbounds i8, i8* %26, i64 0, !dbg !58
  %28 = bitcast i8* %27 to i32**, !dbg !59
  %29 = load i32*, i32** %28, align 8, !dbg !59
  store i32* %29, i32** %8, align 8, !dbg !56
  call void @llvm.dbg.declare(metadata i32** %9, metadata !60, metadata !DIExpression()), !dbg !61
  %30 = load i8*, i8** %6, align 8, !dbg !62
  %31 = getelementptr inbounds i8, i8* %30, i64 8, !dbg !63
  %32 = bitcast i8* %31 to i32**, !dbg !64
  %33 = load i32*, i32** %32, align 8, !dbg !64
  store i32* %33, i32** %9, align 8, !dbg !61
  call void @llvm.dbg.declare(metadata i32** %10, metadata !65, metadata !DIExpression()), !dbg !66
  %34 = load i8*, i8** %6, align 8, !dbg !67
  %35 = getelementptr inbounds i8, i8* %34, i64 16, !dbg !68
  %36 = bitcast i8* %35 to i32**, !dbg !69
  %37 = load i32*, i32** %36, align 8, !dbg !69
  store i32* %37, i32** %10, align 8, !dbg !66
  %38 = load i8*, i8** %5, align 8, !dbg !70
  call void @sea_dsa_set_read(i8* noundef %38), !dbg !71
  %39 = load i8*, i8** %6, align 8, !dbg !72
  call void @sea_dsa_set_modified(i8* noundef %39), !dbg !73
  %40 = load i32*, i32** %8, align 8, !dbg !74
  %41 = icmp ne i32* %40, null, !dbg !75
  %42 = zext i1 %41 to i32, !dbg !75
  %43 = load i32*, i32** %9, align 8, !dbg !76
  %44 = icmp ne i32* %43, null, !dbg !77
  %45 = zext i1 %44 to i32, !dbg !77
  %46 = add nsw i32 %42, %45, !dbg !78
  %47 = load i32*, i32** %10, align 8, !dbg !79
  %48 = icmp ne i32* %47, null, !dbg !80
  %49 = zext i1 %48 to i32, !dbg !80
  %50 = add nsw i32 %46, %49, !dbg !81
  ret i32 %50, !dbg !82
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind
declare noalias i8* @malloc(i64 noundef) #2

declare i64 @get_len() #3

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #4

declare void @sea_dsa_set_read(i8* noundef) #3

declare void @sea_dsa_set_modified(i8* noundef) #3

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #2 = { nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { argmemonly nofree nounwind willreturn }
attributes #5 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!9, !10, !11, !12, !13, !14, !15}
!llvm.ident = !{!16}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "build/c_emp/byte_buf.c", directory: "/home/ljgy/Works/seatools/sea-dsa", checksumkind: CSK_MD5, checksum: "e5036c0885c0b263ba2a2e375961d832")
!2 = !{!3, !5, !8}
!3 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !4, size: 64)
!4 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!5 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !6, size: 64)
!6 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !7, size: 64)
!7 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!8 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!9 = !{i32 7, !"Dwarf Version", i32 5}
!10 = !{i32 2, !"Debug Info Version", i32 3}
!11 = !{i32 1, !"wchar_size", i32 4}
!12 = !{i32 7, !"PIC Level", i32 2}
!13 = !{i32 7, !"PIE Level", i32 2}
!14 = !{i32 7, !"uwtable", i32 1}
!15 = !{i32 7, !"frame-pointer", i32 2}
!16 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
!17 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 9, type: !18, scopeLine: 9, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !20)
!18 = !DISubroutineType(types: !19)
!19 = !{!7}
!20 = !{}
!21 = !DILocalVariable(name: "x0", scope: !17, file: !1, line: 10, type: !7)
!22 = !DILocation(line: 10, column: 7, scope: !17)
!23 = !DILocalVariable(name: "x1", scope: !17, file: !1, line: 10, type: !7)
!24 = !DILocation(line: 10, column: 11, scope: !17)
!25 = !DILocalVariable(name: "x2", scope: !17, file: !1, line: 10, type: !7)
!26 = !DILocation(line: 10, column: 15, scope: !17)
!27 = !DILocalVariable(name: "buf", scope: !17, file: !1, line: 15, type: !3)
!28 = !DILocation(line: 15, column: 9, scope: !17)
!29 = !DILocation(line: 15, column: 23, scope: !17)
!30 = !DILocalVariable(name: "buf2", scope: !17, file: !1, line: 16, type: !3)
!31 = !DILocation(line: 16, column: 9, scope: !17)
!32 = !DILocation(line: 16, column: 24, scope: !17)
!33 = !DILocation(line: 19, column: 13, scope: !17)
!34 = !DILocation(line: 19, column: 17, scope: !17)
!35 = !DILocation(line: 19, column: 3, scope: !17)
!36 = !DILocation(line: 19, column: 22, scope: !17)
!37 = !DILocation(line: 20, column: 13, scope: !17)
!38 = !DILocation(line: 20, column: 17, scope: !17)
!39 = !DILocation(line: 20, column: 3, scope: !17)
!40 = !DILocation(line: 20, column: 22, scope: !17)
!41 = !DILocation(line: 21, column: 13, scope: !17)
!42 = !DILocation(line: 21, column: 17, scope: !17)
!43 = !DILocation(line: 21, column: 3, scope: !17)
!44 = !DILocation(line: 21, column: 23, scope: !17)
!45 = !DILocalVariable(name: "n", scope: !17, file: !1, line: 24, type: !46)
!46 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !47, line: 46, baseType: !48)
!47 = !DIFile(filename: "/usr/lib/llvm-14/lib/clang/14.0.0/include/stddef.h", directory: "", checksumkind: CSK_MD5, checksum: "2499dd2361b915724b073282bea3a7bc")
!48 = !DIBasicType(name: "unsigned long", size: 64, encoding: DW_ATE_unsigned)
!49 = !DILocation(line: 24, column: 10, scope: !17)
!50 = !DILocation(line: 24, column: 14, scope: !17)
!51 = !DILocation(line: 25, column: 10, scope: !17)
!52 = !DILocation(line: 25, column: 16, scope: !17)
!53 = !DILocation(line: 25, column: 21, scope: !17)
!54 = !DILocation(line: 25, column: 3, scope: !17)
!55 = !DILocalVariable(name: "a", scope: !17, file: !1, line: 28, type: !6)
!56 = !DILocation(line: 28, column: 8, scope: !17)
!57 = !DILocation(line: 28, column: 22, scope: !17)
!58 = !DILocation(line: 28, column: 27, scope: !17)
!59 = !DILocation(line: 28, column: 12, scope: !17)
!60 = !DILocalVariable(name: "b", scope: !17, file: !1, line: 29, type: !6)
!61 = !DILocation(line: 29, column: 8, scope: !17)
!62 = !DILocation(line: 29, column: 22, scope: !17)
!63 = !DILocation(line: 29, column: 27, scope: !17)
!64 = !DILocation(line: 29, column: 12, scope: !17)
!65 = !DILocalVariable(name: "c", scope: !17, file: !1, line: 30, type: !6)
!66 = !DILocation(line: 30, column: 8, scope: !17)
!67 = !DILocation(line: 30, column: 22, scope: !17)
!68 = !DILocation(line: 30, column: 27, scope: !17)
!69 = !DILocation(line: 30, column: 12, scope: !17)
!70 = !DILocation(line: 32, column: 20, scope: !17)
!71 = !DILocation(line: 32, column: 3, scope: !17)
!72 = !DILocation(line: 33, column: 24, scope: !17)
!73 = !DILocation(line: 33, column: 3, scope: !17)
!74 = !DILocation(line: 34, column: 11, scope: !17)
!75 = !DILocation(line: 34, column: 13, scope: !17)
!76 = !DILocation(line: 34, column: 25, scope: !17)
!77 = !DILocation(line: 34, column: 27, scope: !17)
!78 = !DILocation(line: 34, column: 22, scope: !17)
!79 = !DILocation(line: 34, column: 39, scope: !17)
!80 = !DILocation(line: 34, column: 41, scope: !17)
!81 = !DILocation(line: 34, column: 36, scope: !17)
!82 = !DILocation(line: 34, column: 3, scope: !17)
