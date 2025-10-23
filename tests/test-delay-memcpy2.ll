; RUN: %seadsa  %butd_dsa --sea-dsa-dot %s --sea-dsa-lazy-mem-transfer --sea-dsa-stats --sea-dsa-type-aware --sea-dsa-dot-outdir=%T/test-delay-memcpy2.ll
; RUN: %cmp-graphs %tests/test-delay-memcpy2.ll.main.mem.dot %T/test-delay-memcpy2.ll/main.mem.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; ModuleID = 'test-delay-memcpy2.ll'
source_filename = "llvm-link"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct.person = type { i8*, i32 }
%struct.byte_buf = type { i32, i32, i8* }
%struct.User = type { i32, %struct.person, %struct.Profile*, i32 }
%struct.Profile = type { i32, %struct.User* }
%struct.double_array = type { i32, [0 x i32] }

@.str = private unnamed_addr constant [5 x i8] c"John\00", align 1
@__const.main.destination2 = private unnamed_addr constant [3 x i32] [i32 1, i32 2, i32 3], align 4
@.str.1 = private unnamed_addr constant [4 x i8] c"%d \00", align 1
@.str.2 = private unnamed_addr constant [6 x i8] c"Alice\00", align 1

; Function Attrs: nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #0 !dbg !44 {
.peel.begin34:
  %0 = alloca i64, align 8
  %1 = alloca i64, align 8
  %2 = alloca i64, align 8
  %3 = alloca i64, align 8
  %4 = alloca i64, align 8
  %5 = alloca i64, align 8
  %6 = alloca i64, align 8
  %7 = alloca %struct.person, align 8
  %8 = alloca %struct.person, align 8
  %9 = alloca [3 x i32], align 4
  %10 = alloca [3 x i32], align 4
  %11 = alloca [3 x i32], align 4
  %12 = alloca %struct.byte_buf, align 8
  %13 = alloca %struct.byte_buf, align 8
  %14 = alloca %struct.User, align 8
  %15 = alloca %struct.Profile, align 8
  %16 = alloca %struct.User, align 8
  %17 = bitcast %struct.person* %7 to i8*, !dbg !104
  call void @llvm.lifetime.start.p0i8(i64 16, i8* nonnull %17) #7, !dbg !104
  call void @llvm.dbg.declare(metadata %struct.person* %7, metadata !50, metadata !DIExpression()), !dbg !105
  %18 = getelementptr inbounds %struct.person, %struct.person* %7, i64 0, i32 0, !dbg !106
  store i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str, i64 0, i64 0), i8** %18, align 8, !dbg !107, !tbaa !108
  %19 = getelementptr inbounds %struct.person, %struct.person* %7, i64 0, i32 1, !dbg !114
  store i32 25, i32* %19, align 8, !dbg !115, !tbaa !116
  %20 = bitcast %struct.person* %8 to i8*, !dbg !117
  call void @llvm.lifetime.start.p0i8(i64 16, i8* nonnull %20) #7, !dbg !117
  call void @llvm.dbg.declare(metadata %struct.person* %8, metadata !55, metadata !DIExpression()), !dbg !118
  %21 = bitcast i64* %6 to i8*, !dbg !119
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %21), !dbg !119
  call void @llvm.dbg.value(metadata i8* %20, metadata !125, metadata !DIExpression()) #7, !dbg !119
  call void @llvm.dbg.value(metadata i8* %17, metadata !126, metadata !DIExpression()) #7, !dbg !119
  call void @llvm.dbg.value(metadata i64 16, metadata !127, metadata !DIExpression()) #7, !dbg !119
  store i64 16, i64* %6, align 8, !tbaa !131
  call void @__CRAB_intrinsic_move_tags(i8* noundef nonnull %17, i8* noundef nonnull %20) #8, !dbg !133
  call void @llvm.dbg.value(metadata i8* %21, metadata !128, metadata !DIExpression()) #7, !dbg !134
  call void @llvm.dbg.value(metadata i64* %6, metadata !127, metadata !DIExpression(DW_OP_deref)) #7, !dbg !119
  call void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef nonnull %21, i64 noundef 1) #8, !dbg !135
  %22 = load i64, i64* %6, align 8, !dbg !136, !tbaa !131
  call void @llvm.dbg.value(metadata i64 %22, metadata !127, metadata !DIExpression()) #7, !dbg !119
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 8 %20, i8* nonnull align 8 %17, i64 %22, i1 false) #7, !dbg !137
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %21), !dbg !138
  %23 = getelementptr inbounds %struct.person, %struct.person* %8, i64 0, i32 1, !dbg !139
  store i32 30, i32* %23, align 8, !dbg !140, !tbaa !116
  %24 = bitcast [3 x i32]* %9 to i8*, !dbg !141
  call void @llvm.lifetime.start.p0i8(i64 12, i8* nonnull %24) #7, !dbg !141
  call void @llvm.dbg.declare(metadata [3 x i32]* %9, metadata !56, metadata !DIExpression()), !dbg !142
  call void @llvm.dbg.value(metadata i32 0, metadata !60, metadata !DIExpression()), !dbg !143
  call void @llvm.dbg.value(metadata i32 0, metadata !60, metadata !DIExpression()), !dbg !143
  %25 = getelementptr inbounds [3 x i32], [3 x i32]* %9, i64 0, i64 0, !dbg !144
  store i32 1, i32* %25, align 4, !dbg !147, !tbaa !148
  call void @llvm.dbg.value(metadata i32 1, metadata !60, metadata !DIExpression()), !dbg !143
  br label %.peel.newph36

.peel.newph36:                                    ; preds = %.peel.begin34, %.peel.newph36
  %sea-indvars.iv39 = phi i64 [ %sea-indvars.iv.next40, %.peel.newph36 ], [ 1, %.peel.begin34 ]
  call void @llvm.dbg.value(metadata i64 %sea-indvars.iv39, metadata !60, metadata !DIExpression()), !dbg !143
  %sea-indvars.iv.next40 = add nuw nsw i64 %sea-indvars.iv39, 1, !dbg !149
  %26 = getelementptr inbounds [3 x i32], [3 x i32]* %9, i64 0, i64 %sea-indvars.iv39, !dbg !144
  %27 = trunc i64 %sea-indvars.iv.next40 to i32, !dbg !147
  store i32 %27, i32* %26, align 4, !dbg !147, !tbaa !148
  call void @llvm.dbg.value(metadata i64 %sea-indvars.iv.next40, metadata !60, metadata !DIExpression()), !dbg !143
  %exitcond2 = icmp ult i64 %sea-indvars.iv39, 2, !dbg !150
  br i1 %exitcond2, label %.peel.newph36, label %.loopexit38, !dbg !151, !llvm.loop !152

.loopexit38:                                      ; preds = %.peel.newph36
  %28 = bitcast [3 x i32]* %10 to i8*, !dbg !157
  call void @llvm.lifetime.start.p0i8(i64 12, i8* nonnull %28) #7, !dbg !157
  call void @llvm.dbg.declare(metadata [3 x i32]* %10, metadata !62, metadata !DIExpression()), !dbg !158
  %29 = bitcast i64* %5 to i8*, !dbg !159
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %29), !dbg !159
  call void @llvm.dbg.value(metadata i8* %28, metadata !125, metadata !DIExpression()) #7, !dbg !159
  call void @llvm.dbg.value(metadata i8* %24, metadata !126, metadata !DIExpression()) #7, !dbg !159
  call void @llvm.dbg.value(metadata i64 12, metadata !127, metadata !DIExpression()) #7, !dbg !159
  store i64 12, i64* %5, align 8, !tbaa !131
  call void @__CRAB_intrinsic_move_tags(i8* noundef nonnull %24, i8* noundef nonnull %28) #8, !dbg !161
  call void @llvm.dbg.value(metadata i8* %29, metadata !128, metadata !DIExpression()) #7, !dbg !162
  call void @llvm.dbg.value(metadata i64* %5, metadata !127, metadata !DIExpression(DW_OP_deref)) #7, !dbg !159
  call void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef nonnull %29, i64 noundef 1) #8, !dbg !163
  %30 = load i64, i64* %5, align 8, !dbg !164, !tbaa !131
  call void @llvm.dbg.value(metadata i64 %30, metadata !127, metadata !DIExpression()) #7, !dbg !159
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 4 %28, i8* nonnull align 4 %24, i64 %30, i1 false) #7, !dbg !165
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %29), !dbg !166
  %31 = bitcast [3 x i32]* %11 to i8*, !dbg !167
  call void @llvm.lifetime.start.p0i8(i64 12, i8* nonnull %31) #7, !dbg !167
  call void @llvm.dbg.declare(metadata [3 x i32]* %11, metadata !63, metadata !DIExpression()), !dbg !168
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* noundef nonnull align 4 dereferenceable(12) %31, i8* noundef nonnull align 4 dereferenceable(12) bitcast ([3 x i32]* @__const.main.destination2 to i8*), i64 12, i1 false), !dbg !168
  %32 = getelementptr inbounds [3 x i32], [3 x i32]* %9, i64 0, i64 1, !dbg !169
  %33 = bitcast i32* %32 to i8*, !dbg !170
  %34 = bitcast i64* %4 to i8*, !dbg !171
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %34), !dbg !171
  call void @llvm.dbg.value(metadata i8* %31, metadata !125, metadata !DIExpression()) #7, !dbg !171
  call void @llvm.dbg.value(metadata i8* %33, metadata !126, metadata !DIExpression()) #7, !dbg !171
  call void @llvm.dbg.value(metadata i64 8, metadata !127, metadata !DIExpression()) #7, !dbg !171
  store i64 8, i64* %4, align 8, !tbaa !131
  call void @__CRAB_intrinsic_move_tags(i8* noundef nonnull %33, i8* noundef nonnull %31) #8, !dbg !173
  call void @llvm.dbg.value(metadata i8* %34, metadata !128, metadata !DIExpression()) #7, !dbg !174
  call void @llvm.dbg.value(metadata i64* %4, metadata !127, metadata !DIExpression(DW_OP_deref)) #7, !dbg !171
  call void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef nonnull %34, i64 noundef 1) #8, !dbg !175
  %35 = load i64, i64* %4, align 8, !dbg !176, !tbaa !131
  call void @llvm.dbg.value(metadata i64 %35, metadata !127, metadata !DIExpression()) #7, !dbg !171
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 4 %31, i8* nonnull align 4 %33, i64 %35, i1 false) #7, !dbg !177
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %34), !dbg !178
  %malloc78 = alloca [4 x %struct.double_array], align 4
  call void @llvm.dbg.value(metadata [4 x %struct.double_array]* %malloc78, metadata !64, metadata !DIExpression()), !dbg !179
  %36 = getelementptr inbounds [4 x %struct.double_array], [4 x %struct.double_array]* %malloc78, i64 0, i64 0, i32 0, !dbg !180
  store i32 3, i32* %36, align 4, !dbg !181, !tbaa !148
  %37 = getelementptr inbounds [4 x %struct.double_array], [4 x %struct.double_array]* %malloc78, i64 0, i64 0, i32 1, i64 0, !dbg !182
  %38 = bitcast i32* %37 to i8*, !dbg !182
  %39 = bitcast i64* %3 to i8*, !dbg !183
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %39), !dbg !183
  call void @llvm.dbg.value(metadata i8* %38, metadata !125, metadata !DIExpression()) #7, !dbg !183
  call void @llvm.dbg.value(metadata i8* %24, metadata !126, metadata !DIExpression()) #7, !dbg !183
  call void @llvm.dbg.value(metadata i64 12, metadata !127, metadata !DIExpression()) #7, !dbg !183
  store i64 12, i64* %3, align 8, !tbaa !131
  call void @__CRAB_intrinsic_move_tags(i8* noundef nonnull %24, i8* noundef nonnull %38) #8, !dbg !185
  call void @llvm.dbg.value(metadata i8* %39, metadata !128, metadata !DIExpression()) #7, !dbg !186
  call void @llvm.dbg.value(metadata i64* %3, metadata !127, metadata !DIExpression(DW_OP_deref)) #7, !dbg !183
  call void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef nonnull %39, i64 noundef 1) #8, !dbg !187
  %40 = load i64, i64* %3, align 8, !dbg !188, !tbaa !131
  call void @llvm.dbg.value(metadata i64 %40, metadata !127, metadata !DIExpression()) #7, !dbg !183
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 4 %38, i8* nonnull align 4 %24, i64 %40, i1 false) #7, !dbg !189
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %39), !dbg !190
  %malloc1910 = alloca [3 x i32], align 4
  call void @llvm.dbg.value(metadata [3 x i32]* %malloc1910, metadata !73, metadata !DIExpression()), !dbg !179
  call void @llvm.dbg.value(metadata i32 0, metadata !75, metadata !DIExpression()), !dbg !191
  call void @llvm.dbg.value(metadata i32 0, metadata !75, metadata !DIExpression()), !dbg !191
  %41 = getelementptr inbounds [3 x i32], [3 x i32]* %malloc1910, i64 0, i64 0, !dbg !192
  store i32 1, i32* %41, align 4, !dbg !195, !tbaa !148
  call void @llvm.dbg.value(metadata i32 1, metadata !75, metadata !DIExpression()), !dbg !191
  br label %.peel.newph28

.peel.newph28:                                    ; preds = %.loopexit38, %.peel.newph28
  %sea-indvars.iv31 = phi i64 [ %sea-indvars.iv.next32, %.peel.newph28 ], [ 1, %.loopexit38 ]
  call void @llvm.dbg.value(metadata i64 %sea-indvars.iv31, metadata !75, metadata !DIExpression()), !dbg !191
  %sea-indvars.iv.next32 = add nuw nsw i64 %sea-indvars.iv31, 1, !dbg !196
  %42 = getelementptr inbounds [3 x i32], [3 x i32]* %malloc1910, i64 0, i64 %sea-indvars.iv31, !dbg !192
  %43 = trunc i64 %sea-indvars.iv.next32 to i32, !dbg !195
  store i32 %43, i32* %42, align 4, !dbg !195, !tbaa !148
  call void @llvm.dbg.value(metadata i64 %sea-indvars.iv.next32, metadata !75, metadata !DIExpression()), !dbg !191
  %exitcond = icmp ult i64 %sea-indvars.iv31, 2, !dbg !197
  br i1 %exitcond, label %.peel.newph28, label %.loopexit30, !dbg !198, !llvm.loop !199

.loopexit30:                                      ; preds = %.peel.newph28
  %malloc21112 = alloca [5 x i32], align 4
  call void @llvm.dbg.value(metadata [5 x i32]* %malloc21112, metadata !77, metadata !DIExpression()), !dbg !179
  %44 = getelementptr inbounds [5 x i32], [5 x i32]* %malloc21112, i64 0, i64 3, !dbg !201
  store i32 4, i32* %44, align 4, !dbg !202, !tbaa !148
  %45 = getelementptr inbounds [5 x i32], [5 x i32]* %malloc21112, i64 0, i64 4, !dbg !203
  store i32 5, i32* %45, align 4, !dbg !204, !tbaa !148
  %46 = bitcast [5 x i32]* %malloc21112 to i8*, !dbg !205
  %47 = bitcast [3 x i32]* %malloc1910 to i8*, !dbg !206
  %48 = bitcast i64* %2 to i8*, !dbg !207
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %48), !dbg !207
  call void @llvm.dbg.value(metadata i8* %46, metadata !125, metadata !DIExpression()) #7, !dbg !207
  call void @llvm.dbg.value(metadata i8* %47, metadata !126, metadata !DIExpression()) #7, !dbg !207
  call void @llvm.dbg.value(metadata i64 12, metadata !127, metadata !DIExpression()) #7, !dbg !207
  store i64 12, i64* %2, align 8, !tbaa !131
  call void @__CRAB_intrinsic_move_tags(i8* noundef nonnull %47, i8* noundef nonnull %46) #8, !dbg !209
  call void @llvm.dbg.value(metadata i8* %48, metadata !128, metadata !DIExpression()) #7, !dbg !210
  call void @llvm.dbg.value(metadata i64* %2, metadata !127, metadata !DIExpression(DW_OP_deref)) #7, !dbg !207
  call void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef nonnull %48, i64 noundef 1) #8, !dbg !211
  %49 = load i64, i64* %2, align 8, !dbg !212, !tbaa !131
  call void @llvm.dbg.value(metadata i64 %49, metadata !127, metadata !DIExpression()) #7, !dbg !207
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 4 %46, i8* nonnull align 4 %47, i64 %49, i1 false) #7, !dbg !213
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %48), !dbg !214
  call void @llvm.dbg.value(metadata i32 0, metadata !78, metadata !DIExpression()), !dbg !215
  call void @llvm.dbg.value(metadata i32 0, metadata !78, metadata !DIExpression()), !dbg !215
  %50 = getelementptr inbounds [5 x i32], [5 x i32]* %malloc21112, i64 0, i64 0, !dbg !216
  %51 = load i32, i32* %50, align 4, !dbg !216, !tbaa !148
  %52 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i64 0, i64 0), i32 noundef %51) #8, !dbg !219
  call void @llvm.dbg.value(metadata i32 1, metadata !78, metadata !DIExpression()), !dbg !215
  br label %.peel.newph21

.peel.newph21:                                    ; preds = %.loopexit30, %.peel.newph21
  %sea-indvars.iv24 = phi i64 [ %sea-indvars.iv.next25, %.peel.newph21 ], [ 1, %.loopexit30 ]
  call void @llvm.dbg.value(metadata i64 %sea-indvars.iv24, metadata !78, metadata !DIExpression()), !dbg !215
  %53 = getelementptr inbounds [5 x i32], [5 x i32]* %malloc21112, i64 0, i64 %sea-indvars.iv24, !dbg !216
  %54 = load i32, i32* %53, align 4, !dbg !216, !tbaa !148
  %55 = call i32 (i8*, ...) @printf(i8* noundef nonnull dereferenceable(1) getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i64 0, i64 0), i32 noundef %54) #8, !dbg !219
  %sea-indvars.iv.next25 = add nuw nsw i64 %sea-indvars.iv24, 1, !dbg !220
  call void @llvm.dbg.value(metadata i64 %sea-indvars.iv.next25, metadata !78, metadata !DIExpression()), !dbg !215
  %exitcond1 = icmp ult i64 %sea-indvars.iv24, 4, !dbg !221
  br i1 %exitcond1, label %.peel.newph21, label %.loopexit23, !dbg !222, !llvm.loop !223

.loopexit23:                                      ; preds = %.peel.newph21
  %56 = bitcast %struct.byte_buf* %12 to i8*, !dbg !225
  call void @llvm.lifetime.start.p0i8(i64 16, i8* nonnull %56) #7, !dbg !225
  call void @llvm.dbg.declare(metadata %struct.byte_buf* %12, metadata !80, metadata !DIExpression()), !dbg !226
  %57 = getelementptr inbounds %struct.byte_buf, %struct.byte_buf* %12, i64 0, i32 0, !dbg !227
  store i32 5, i32* %57, align 8, !dbg !228, !tbaa !229
  %58 = getelementptr inbounds %struct.byte_buf, %struct.byte_buf* %12, i64 0, i32 1, !dbg !231
  store i32 10, i32* %58, align 4, !dbg !232, !tbaa !233
  %malloc313 = alloca [10 x i8], align 1
  %malloc313.sub = getelementptr inbounds [10 x i8], [10 x i8]* %malloc313, i64 0, i64 0
  %59 = getelementptr inbounds %struct.byte_buf, %struct.byte_buf* %12, i64 0, i32 2, !dbg !234
  store i8* %malloc313.sub, i8** %59, align 8, !dbg !235, !tbaa !236
  call void @llvm.dbg.value(metadata i32 0, metadata !86, metadata !DIExpression()), !dbg !237
  call void @llvm.dbg.value(metadata i32 0, metadata !86, metadata !DIExpression()), !dbg !237
  store i8 1, i8* %malloc313.sub, align 1, !dbg !238, !tbaa !241
  call void @llvm.dbg.value(metadata i32 1, metadata !86, metadata !DIExpression()), !dbg !237
  br label %.peel.newph, !dbg !242

.peel.newph:                                      ; preds = %.loopexit23, %.peel.newph
  %sea-indvars.iv = phi i64 [ %sea-indvars.iv.next, %.peel.newph ], [ 1, %.loopexit23 ]
  call void @llvm.dbg.value(metadata i64 %sea-indvars.iv, metadata !86, metadata !DIExpression()), !dbg !237
  %60 = trunc i64 %sea-indvars.iv to i8, !dbg !243
  %61 = add i8 %60, 1, !dbg !243
  %62 = load i8*, i8** %59, align 8, !dbg !244, !tbaa !236
  %63 = getelementptr inbounds i8, i8* %62, i64 %sea-indvars.iv, !dbg !245
  store i8 %61, i8* %63, align 1, !dbg !238, !tbaa !241
  %sea-indvars.iv.next = add nuw nsw i64 %sea-indvars.iv, 1, !dbg !246
  call void @llvm.dbg.value(metadata i64 %sea-indvars.iv.next, metadata !86, metadata !DIExpression()), !dbg !237
  %.pre = load i32, i32* %57, align 8, !dbg !247, !tbaa !229
  %64 = sext i32 %.pre to i64, !dbg !248
  %65 = icmp slt i64 %sea-indvars.iv.next, %64, !dbg !248
  br i1 %65, label %.peel.newph, label %.loopexit, !dbg !242, !llvm.loop !249

.loopexit:                                        ; preds = %.peel.newph
  %66 = bitcast %struct.byte_buf* %13 to i8*, !dbg !251
  call void @llvm.lifetime.start.p0i8(i64 16, i8* nonnull %66) #7, !dbg !251
  call void @llvm.dbg.declare(metadata %struct.byte_buf* %13, metadata !88, metadata !DIExpression()), !dbg !252
  %67 = bitcast i64* %1 to i8*, !dbg !253
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %67), !dbg !253
  call void @llvm.dbg.value(metadata i8* %66, metadata !125, metadata !DIExpression()) #7, !dbg !253
  call void @llvm.dbg.value(metadata i8* %56, metadata !126, metadata !DIExpression()) #7, !dbg !253
  call void @llvm.dbg.value(metadata i64 16, metadata !127, metadata !DIExpression()) #7, !dbg !253
  store i64 16, i64* %1, align 8, !tbaa !131
  call void @__CRAB_intrinsic_move_tags(i8* noundef nonnull %56, i8* noundef nonnull %66) #8, !dbg !255
  call void @llvm.dbg.value(metadata i8* %67, metadata !128, metadata !DIExpression()) #7, !dbg !256
  call void @llvm.dbg.value(metadata i64* %1, metadata !127, metadata !DIExpression(DW_OP_deref)) #7, !dbg !253
  call void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef nonnull %67, i64 noundef 1) #8, !dbg !257
  %68 = load i64, i64* %1, align 8, !dbg !258, !tbaa !131
  call void @llvm.dbg.value(metadata i64 %68, metadata !127, metadata !DIExpression()) #7, !dbg !253
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 8 %66, i8* nonnull align 8 %56, i64 %68, i1 false) #7, !dbg !259
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %67), !dbg !260
  %69 = getelementptr inbounds %struct.byte_buf, %struct.byte_buf* %13, i64 0, i32 1, !dbg !261
  %70 = load i32, i32* %69, align 4, !dbg !261, !tbaa !233
  %71 = icmp slt i32 %70, 10, !dbg !263
  br i1 %71, label %72, label %76, !dbg !264

72:                                               ; preds = %.loopexit
  store i32 20, i32* %69, align 4, !dbg !265, !tbaa !233
  %73 = getelementptr inbounds %struct.byte_buf, %struct.byte_buf* %13, i64 0, i32 2, !dbg !267
  %74 = load i8*, i8** %73, align 8, !dbg !267, !tbaa !236
  %75 = call dereferenceable_or_null(20) i8* @realloc(i8* noundef %74, i64 noundef 20) #8, !dbg !268
  store i8* %75, i8** %73, align 8, !dbg !269, !tbaa !236
  br label %76, !dbg !270

76:                                               ; preds = %72, %.loopexit
  %77 = bitcast %struct.User* %14 to i8*, !dbg !271
  call void @llvm.lifetime.start.p0i8(i64 40, i8* nonnull %77) #7, !dbg !271
  call void @llvm.dbg.declare(metadata %struct.User* %14, metadata !89, metadata !DIExpression()), !dbg !272
  %78 = getelementptr inbounds %struct.User, %struct.User* %14, i64 0, i32 0, !dbg !273
  store i32 1, i32* %78, align 8, !dbg !274, !tbaa !275
  %79 = getelementptr inbounds %struct.User, %struct.User* %14, i64 0, i32 1, i32 0, !dbg !277
  store i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.2, i64 0, i64 0), i8** %79, align 8, !dbg !278, !tbaa !279
  %80 = getelementptr inbounds %struct.User, %struct.User* %14, i64 0, i32 1, i32 1, !dbg !280
  store i32 28, i32* %80, align 8, !dbg !281, !tbaa !282
  %81 = getelementptr inbounds %struct.User, %struct.User* %14, i64 0, i32 3, !dbg !283
  store i32 5, i32* %81, align 8, !dbg !284, !tbaa !285
  %82 = bitcast %struct.Profile* %15 to i8*, !dbg !286
  call void @llvm.lifetime.start.p0i8(i64 16, i8* nonnull %82) #7, !dbg !286
  call void @llvm.dbg.declare(metadata %struct.Profile* %15, metadata !102, metadata !DIExpression()), !dbg !287
  %83 = getelementptr inbounds %struct.Profile, %struct.Profile* %15, i64 0, i32 0, !dbg !288
  store i32 101, i32* %83, align 8, !dbg !289, !tbaa !290
  %84 = getelementptr inbounds %struct.Profile, %struct.Profile* %15, i64 0, i32 1, !dbg !292
  store %struct.User* %14, %struct.User** %84, align 8, !dbg !293, !tbaa !294
  %85 = getelementptr inbounds %struct.User, %struct.User* %14, i64 0, i32 2, !dbg !295
  store %struct.Profile* %15, %struct.Profile** %85, align 8, !dbg !296, !tbaa !297
  %86 = bitcast %struct.User* %16 to i8*, !dbg !298
  call void @llvm.lifetime.start.p0i8(i64 40, i8* nonnull %86) #7, !dbg !298
  call void @llvm.dbg.declare(metadata %struct.User* %16, metadata !103, metadata !DIExpression()), !dbg !299
  %87 = bitcast i64* %0 to i8*, !dbg !300
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %87), !dbg !300
  call void @llvm.dbg.value(metadata i8* %86, metadata !125, metadata !DIExpression()) #7, !dbg !300
  call void @llvm.dbg.value(metadata i8* %77, metadata !126, metadata !DIExpression()) #7, !dbg !300
  call void @llvm.dbg.value(metadata i64 40, metadata !127, metadata !DIExpression()) #7, !dbg !300
  store i64 40, i64* %0, align 8, !tbaa !131
  call void @__CRAB_intrinsic_move_tags(i8* noundef nonnull %77, i8* noundef nonnull %86) #8, !dbg !302
  call void @llvm.dbg.value(metadata i8* %87, metadata !128, metadata !DIExpression()) #7, !dbg !303
  call void @llvm.dbg.value(metadata i64* %0, metadata !127, metadata !DIExpression(DW_OP_deref)) #7, !dbg !300
  call void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef nonnull %87, i64 noundef 1) #8, !dbg !304
  %88 = load i64, i64* %0, align 8, !dbg !305, !tbaa !131
  call void @llvm.dbg.value(metadata i64 %88, metadata !127, metadata !DIExpression()) #7, !dbg !300
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 8 %86, i8* nonnull align 8 %77, i64 %88, i1 false) #7, !dbg !306
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %87), !dbg !307
  call void @llvm.lifetime.end.p0i8(i64 40, i8* nonnull %86) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 16, i8* nonnull %82) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 40, i8* nonnull %77) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 16, i8* nonnull %66) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 16, i8* nonnull %56) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 12, i8* nonnull %31) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 12, i8* nonnull %28) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 12, i8* nonnull %24) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 16, i8* nonnull %20) #7, !dbg !308
  call void @llvm.lifetime.end.p0i8(i64 16, i8* nonnull %17) #7, !dbg !308
  ret i32 0, !dbg !309
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #2

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #3

; Function Attrs: nofree nounwind
declare noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #4

; Function Attrs: inaccessiblemem_or_argmemonly mustprogress nounwind willreturn
declare noalias noundef i8* @realloc(i8* nocapture noundef, i64 noundef) local_unnamed_addr #5

declare void @__CRAB_intrinsic_move_tags(i8* noundef, i8* noundef) local_unnamed_addr #6

declare void @__CRAB_intrinsic_check_does_not_have_tag(i8* noundef, i64 noundef) local_unnamed_addr #6

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #2

attributes #0 = { nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #3 = { argmemonly nofree nounwind willreturn }
attributes #4 = { nofree nounwind "frame-pointer"="none" "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { inaccessiblemem_or_argmemonly mustprogress nounwind willreturn "frame-pointer"="none" "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #6 = { "frame-pointer"="none" "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #7 = { nounwind }
attributes #8 = { nounwind "no-builtin-memcmp" "no-builtin-memcpy" "no-builtin-memmove" "no-builtin-memset" }

!llvm.dbg.cu = !{!0, !4, !23, !27, !32}
!llvm.ident = !{!37, !37, !37, !37, !37}
!llvm.module.flags = !{!38, !39, !40, !41, !42, !43}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/benchmarks/field/field9/field9.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/benchmarks/field/field9", checksumkind: CSK_MD5, checksum: "0ae7829c0b42401c118b7290e1b507fe")
!2 = !{!3}
!3 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!4 = distinct !DICompileUnit(language: DW_LANG_C99, file: !5, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !6, splitDebugInlining: false, nameTableKind: None)
!5 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/lib/nd_clam.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/lib", checksumkind: CSK_MD5, checksum: "a3e9bf8d46f9e3f9034b51583e568cfb")
!6 = !{!7, !10, !15, !18, !21, !3, !14}
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
!23 = distinct !DICompileUnit(language: DW_LANG_C99, file: !24, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !25, splitDebugInlining: false, nameTableKind: None)
!24 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/lib/sea_allocators.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/lib", checksumkind: CSK_MD5, checksum: "195a095d3b3f671498f7ab8158e09f3f")
!25 = !{!26}
!26 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!27 = distinct !DICompileUnit(language: DW_LANG_C99, file: !28, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !29, splitDebugInlining: false, nameTableKind: None)
!28 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/lib/clam_taint.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/lib", checksumkind: CSK_MD5, checksum: "b6c78d8c3355cb659d9cac7f416aa168")
!29 = !{!30, !31}
!30 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !3, size: 64)
!31 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!32 = distinct !DICompileUnit(language: DW_LANG_C99, file: !33, producer: "Ubuntu clang version 14.0.0-1ubuntu1.1", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, retainedTypes: !34, splitDebugInlining: false, nameTableKind: None)
!33 = !DIFile(filename: "/home/ljgy/Works/seatools/taint-benchs/lib/lib_func.c", directory: "/home/ljgy/Works/seatools/taint-benchs/build/lib", checksumkind: CSK_MD5, checksum: "889ddfbf93ec8cf966ffe2347537d028")
!34 = !{!35}
!35 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !36, size: 64)
!36 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!37 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
!38 = !{i32 7, !"Dwarf Version", i32 5}
!39 = !{i32 2, !"Debug Info Version", i32 3}
!40 = !{i32 1, !"wchar_size", i32 4}
!41 = !{i32 7, !"PIC Level", i32 2}
!42 = !{i32 7, !"PIE Level", i32 2}
!43 = !{i32 7, !"uwtable", i32 1}
!44 = distinct !DISubprogram(name: "main", scope: !45, file: !45, line: 45, type: !46, scopeLine: 45, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !49)
!45 = !DIFile(filename: "benchmarks/field/field9/field9.c", directory: "/home/ljgy/Works/seatools/taint-benchs", checksumkind: CSK_MD5, checksum: "0ae7829c0b42401c118b7290e1b507fe")
!46 = !DISubroutineType(types: !47)
!47 = !{!48}
!48 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!49 = !{!50, !55, !56, !60, !62, !63, !64, !73, !75, !77, !78, !80, !86, !88, !89, !102, !103}
!50 = !DILocalVariable(name: "person1", scope: !44, file: !45, line: 47, type: !51)
!51 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "person", file: !45, line: 14, size: 128, elements: !52)
!52 = !{!53, !54}
!53 = !DIDerivedType(tag: DW_TAG_member, name: "name", scope: !51, file: !45, line: 15, baseType: !30, size: 64)
!54 = !DIDerivedType(tag: DW_TAG_member, name: "age", scope: !51, file: !45, line: 16, baseType: !48, size: 32, offset: 64)
!55 = !DILocalVariable(name: "person2", scope: !44, file: !45, line: 51, type: !51)
!56 = !DILocalVariable(name: "source", scope: !44, file: !45, line: 56, type: !57)
!57 = !DICompositeType(tag: DW_TAG_array_type, baseType: !48, size: 96, elements: !58)
!58 = !{!59}
!59 = !DISubrange(count: 3)
!60 = !DILocalVariable(name: "i", scope: !61, file: !45, line: 57, type: !48)
!61 = distinct !DILexicalBlock(scope: !44, file: !45, line: 57, column: 3)
!62 = !DILocalVariable(name: "destination", scope: !44, file: !45, line: 61, type: !57)
!63 = !DILocalVariable(name: "destination2", scope: !44, file: !45, line: 63, type: !57)
!64 = !DILocalVariable(name: "darr1", scope: !44, file: !45, line: 65, type: !65)
!65 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !66, size: 64)
!66 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "double_array", file: !45, line: 19, size: 32, elements: !67)
!67 = !{!68, !69}
!68 = !DIDerivedType(tag: DW_TAG_member, name: "len", scope: !66, file: !45, line: 20, baseType: !48, size: 32)
!69 = !DIDerivedType(tag: DW_TAG_member, name: "arr", scope: !66, file: !45, line: 21, baseType: !70, offset: 32)
!70 = !DICompositeType(tag: DW_TAG_array_type, baseType: !48, elements: !71)
!71 = !{!72}
!72 = !DISubrange(count: -1)
!73 = !DILocalVariable(name: "old_array", scope: !44, file: !45, line: 69, type: !74)
!74 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !48, size: 64)
!75 = !DILocalVariable(name: "i", scope: !76, file: !45, line: 70, type: !48)
!76 = distinct !DILexicalBlock(scope: !44, file: !45, line: 70, column: 3)
!77 = !DILocalVariable(name: "new_array", scope: !44, file: !45, line: 73, type: !74)
!78 = !DILocalVariable(name: "i", scope: !79, file: !45, line: 76, type: !48)
!79 = distinct !DILexicalBlock(scope: !44, file: !45, line: 76, column: 3)
!80 = !DILocalVariable(name: "buf1", scope: !44, file: !45, line: 81, type: !81)
!81 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "byte_buf", file: !45, line: 26, size: 128, elements: !82)
!82 = !{!83, !84, !85}
!83 = !DIDerivedType(tag: DW_TAG_member, name: "len", scope: !81, file: !45, line: 27, baseType: !48, size: 32)
!84 = !DIDerivedType(tag: DW_TAG_member, name: "size", scope: !81, file: !45, line: 28, baseType: !48, size: 32, offset: 32)
!85 = !DIDerivedType(tag: DW_TAG_member, name: "buf", scope: !81, file: !45, line: 29, baseType: !30, size: 64, offset: 64)
!86 = !DILocalVariable(name: "i", scope: !87, file: !45, line: 85, type: !48)
!87 = distinct !DILexicalBlock(scope: !44, file: !45, line: 85, column: 3)
!88 = !DILocalVariable(name: "buf2", scope: !44, file: !45, line: 88, type: !81)
!89 = !DILocalVariable(name: "user1", scope: !44, file: !45, line: 96, type: !90)
!90 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "User", file: !45, line: 33, size: 320, elements: !91)
!91 = !{!92, !93, !94, !101}
!92 = !DIDerivedType(tag: DW_TAG_member, name: "user_id", scope: !90, file: !45, line: 34, baseType: !48, size: 32)
!93 = !DIDerivedType(tag: DW_TAG_member, name: "personal_info", scope: !90, file: !45, line: 35, baseType: !51, size: 128, offset: 64)
!94 = !DIDerivedType(tag: DW_TAG_member, name: "profile", scope: !90, file: !45, line: 36, baseType: !95, size: 64, offset: 192)
!95 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !96, size: 64)
!96 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Profile", file: !45, line: 40, size: 128, elements: !97)
!97 = !{!98, !99}
!98 = !DIDerivedType(tag: DW_TAG_member, name: "profile_id", scope: !96, file: !45, line: 41, baseType: !48, size: 32)
!99 = !DIDerivedType(tag: DW_TAG_member, name: "owner", scope: !96, file: !45, line: 42, baseType: !100, size: 64, offset: 64)
!100 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !90, size: 64)
!101 = !DIDerivedType(tag: DW_TAG_member, name: "post_count", scope: !90, file: !45, line: 37, baseType: !48, size: 32, offset: 256)
!102 = !DILocalVariable(name: "profile1", scope: !44, file: !45, line: 101, type: !96)
!103 = !DILocalVariable(name: "user2", scope: !44, file: !45, line: 106, type: !90)
!104 = !DILocation(line: 47, column: 3, scope: !44)
!105 = !DILocation(line: 47, column: 17, scope: !44)
!106 = !DILocation(line: 48, column: 11, scope: !44)
!107 = !DILocation(line: 48, column: 16, scope: !44)
!108 = !{!109, !110, i64 0}
!109 = !{!"person", !110, i64 0, !113, i64 8}
!110 = !{!"any pointer", !111, i64 0}
!111 = !{!"omnipotent char", !112, i64 0}
!112 = !{!"Simple C/C++ TBAA"}
!113 = !{!"int", !111, i64 0}
!114 = !DILocation(line: 49, column: 11, scope: !44)
!115 = !DILocation(line: 49, column: 15, scope: !44)
!116 = !{!109, !113, i64 8}
!117 = !DILocation(line: 51, column: 3, scope: !44)
!118 = !DILocation(line: 51, column: 17, scope: !44)
!119 = !DILocation(line: 0, scope: !120, inlinedAt: !130)
!120 = distinct !DISubprogram(name: "memcpy", scope: !121, file: !121, line: 31, type: !122, scopeLine: 31, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !32, retainedNodes: !124)
!121 = !DIFile(filename: "lib/lib_func.c", directory: "/home/ljgy/Works/seatools/taint-benchs", checksumkind: CSK_MD5, checksum: "889ddfbf93ec8cf966ffe2347537d028")
!122 = !DISubroutineType(types: !123)
!123 = !{!26, !26, !35, !7}
!124 = !{!125, !126, !127, !128}
!125 = !DILocalVariable(name: "dst", arg: 1, scope: !120, file: !121, line: 31, type: !26)
!126 = !DILocalVariable(name: "src", arg: 2, scope: !120, file: !121, line: 31, type: !35)
!127 = !DILocalVariable(name: "len", arg: 3, scope: !120, file: !121, line: 31, type: !7)
!128 = !DILocalVariable(name: "__sea_is_not_tainted_ptr", scope: !129, file: !121, line: 34, type: !35)
!129 = distinct !DILexicalBlock(scope: !120, file: !121, line: 34, column: 3)
!130 = distinct !DILocation(line: 52, column: 3, scope: !44)
!131 = !{!132, !132, i64 0}
!132 = !{!"long", !111, i64 0}
!133 = !DILocation(line: 33, column: 3, scope: !120, inlinedAt: !130)
!134 = !DILocation(line: 0, scope: !129, inlinedAt: !130)
!135 = !DILocation(line: 34, column: 3, scope: !129, inlinedAt: !130)
!136 = !DILocation(line: 38, column: 37, scope: !120, inlinedAt: !130)
!137 = !DILocation(line: 38, column: 10, scope: !120, inlinedAt: !130)
!138 = !DILocation(line: 38, column: 3, scope: !120, inlinedAt: !130)
!139 = !DILocation(line: 53, column: 11, scope: !44)
!140 = !DILocation(line: 53, column: 15, scope: !44)
!141 = !DILocation(line: 56, column: 3, scope: !44)
!142 = !DILocation(line: 56, column: 7, scope: !44)
!143 = !DILocation(line: 0, scope: !61)
!144 = !DILocation(line: 58, column: 5, scope: !145)
!145 = distinct !DILexicalBlock(scope: !146, file: !45, line: 57, column: 31)
!146 = distinct !DILexicalBlock(scope: !61, file: !45, line: 57, column: 3)
!147 = !DILocation(line: 58, column: 15, scope: !145)
!148 = !{!113, !113, i64 0}
!149 = !DILocation(line: 58, column: 19, scope: !145)
!150 = !DILocation(line: 57, column: 21, scope: !146)
!151 = !DILocation(line: 57, column: 3, scope: !61)
!152 = distinct !{!152, !151, !153, !154, !155, !156}
!153 = !DILocation(line: 59, column: 3, scope: !61)
!154 = !{!"llvm.loop.mustprogress"}
!155 = !{!"llvm.loop.unroll.disable"}
!156 = !{!"llvm.loop.peeled.count", i32 1}
!157 = !DILocation(line: 61, column: 3, scope: !44)
!158 = !DILocation(line: 61, column: 7, scope: !44)
!159 = !DILocation(line: 0, scope: !120, inlinedAt: !160)
!160 = distinct !DILocation(line: 62, column: 3, scope: !44)
!161 = !DILocation(line: 33, column: 3, scope: !120, inlinedAt: !160)
!162 = !DILocation(line: 0, scope: !129, inlinedAt: !160)
!163 = !DILocation(line: 34, column: 3, scope: !129, inlinedAt: !160)
!164 = !DILocation(line: 38, column: 37, scope: !120, inlinedAt: !160)
!165 = !DILocation(line: 38, column: 10, scope: !120, inlinedAt: !160)
!166 = !DILocation(line: 38, column: 3, scope: !120, inlinedAt: !160)
!167 = !DILocation(line: 63, column: 3, scope: !44)
!168 = !DILocation(line: 63, column: 7, scope: !44)
!169 = !DILocation(line: 64, column: 31, scope: !44)
!170 = !DILocation(line: 64, column: 24, scope: !44)
!171 = !DILocation(line: 0, scope: !120, inlinedAt: !172)
!172 = distinct !DILocation(line: 64, column: 3, scope: !44)
!173 = !DILocation(line: 33, column: 3, scope: !120, inlinedAt: !172)
!174 = !DILocation(line: 0, scope: !129, inlinedAt: !172)
!175 = !DILocation(line: 34, column: 3, scope: !129, inlinedAt: !172)
!176 = !DILocation(line: 38, column: 37, scope: !120, inlinedAt: !172)
!177 = !DILocation(line: 38, column: 10, scope: !120, inlinedAt: !172)
!178 = !DILocation(line: 38, column: 3, scope: !120, inlinedAt: !172)
!179 = !DILocation(line: 0, scope: !44)
!180 = !DILocation(line: 67, column: 10, scope: !44)
!181 = !DILocation(line: 67, column: 14, scope: !44)
!182 = !DILocation(line: 68, column: 10, scope: !44)
!183 = !DILocation(line: 0, scope: !120, inlinedAt: !184)
!184 = distinct !DILocation(line: 68, column: 3, scope: !44)
!185 = !DILocation(line: 33, column: 3, scope: !120, inlinedAt: !184)
!186 = !DILocation(line: 0, scope: !129, inlinedAt: !184)
!187 = !DILocation(line: 34, column: 3, scope: !129, inlinedAt: !184)
!188 = !DILocation(line: 38, column: 37, scope: !120, inlinedAt: !184)
!189 = !DILocation(line: 38, column: 10, scope: !120, inlinedAt: !184)
!190 = !DILocation(line: 38, column: 3, scope: !120, inlinedAt: !184)
!191 = !DILocation(line: 0, scope: !76)
!192 = !DILocation(line: 71, column: 5, scope: !193)
!193 = distinct !DILexicalBlock(scope: !194, file: !45, line: 70, column: 31)
!194 = distinct !DILexicalBlock(scope: !76, file: !45, line: 70, column: 3)
!195 = !DILocation(line: 71, column: 18, scope: !193)
!196 = !DILocation(line: 71, column: 22, scope: !193)
!197 = !DILocation(line: 70, column: 21, scope: !194)
!198 = !DILocation(line: 70, column: 3, scope: !76)
!199 = distinct !{!199, !198, !200, !154, !155, !156}
!200 = !DILocation(line: 72, column: 3, scope: !76)
!201 = !DILocation(line: 74, column: 3, scope: !44)
!202 = !DILocation(line: 74, column: 16, scope: !44)
!203 = !DILocation(line: 74, column: 21, scope: !44)
!204 = !DILocation(line: 74, column: 34, scope: !44)
!205 = !DILocation(line: 75, column: 10, scope: !44)
!206 = !DILocation(line: 75, column: 21, scope: !44)
!207 = !DILocation(line: 0, scope: !120, inlinedAt: !208)
!208 = distinct !DILocation(line: 75, column: 3, scope: !44)
!209 = !DILocation(line: 33, column: 3, scope: !120, inlinedAt: !208)
!210 = !DILocation(line: 0, scope: !129, inlinedAt: !208)
!211 = !DILocation(line: 34, column: 3, scope: !129, inlinedAt: !208)
!212 = !DILocation(line: 38, column: 37, scope: !120, inlinedAt: !208)
!213 = !DILocation(line: 38, column: 10, scope: !120, inlinedAt: !208)
!214 = !DILocation(line: 38, column: 3, scope: !120, inlinedAt: !208)
!215 = !DILocation(line: 0, scope: !79)
!216 = !DILocation(line: 77, column: 19, scope: !217)
!217 = distinct !DILexicalBlock(scope: !218, file: !45, line: 76, column: 31)
!218 = distinct !DILexicalBlock(scope: !79, file: !45, line: 76, column: 3)
!219 = !DILocation(line: 77, column: 5, scope: !217)
!220 = !DILocation(line: 76, column: 27, scope: !218)
!221 = !DILocation(line: 76, column: 21, scope: !218)
!222 = !DILocation(line: 76, column: 3, scope: !79)
!223 = distinct !{!223, !222, !224, !154, !155, !156}
!224 = !DILocation(line: 78, column: 3, scope: !79)
!225 = !DILocation(line: 81, column: 3, scope: !44)
!226 = !DILocation(line: 81, column: 19, scope: !44)
!227 = !DILocation(line: 82, column: 8, scope: !44)
!228 = !DILocation(line: 82, column: 12, scope: !44)
!229 = !{!230, !113, i64 0}
!230 = !{!"byte_buf", !113, i64 0, !113, i64 4, !110, i64 8}
!231 = !DILocation(line: 83, column: 8, scope: !44)
!232 = !DILocation(line: 83, column: 13, scope: !44)
!233 = !{!230, !113, i64 4}
!234 = !DILocation(line: 84, column: 8, scope: !44)
!235 = !DILocation(line: 84, column: 12, scope: !44)
!236 = !{!230, !110, i64 8}
!237 = !DILocation(line: 0, scope: !87)
!238 = !DILocation(line: 86, column: 17, scope: !239)
!239 = distinct !DILexicalBlock(scope: !240, file: !45, line: 85, column: 38)
!240 = distinct !DILexicalBlock(scope: !87, file: !45, line: 85, column: 3)
!241 = !{!111, !111, i64 0}
!242 = !DILocation(line: 85, column: 3, scope: !87)
!243 = !DILocation(line: 86, column: 19, scope: !239)
!244 = !DILocation(line: 86, column: 10, scope: !239)
!245 = !DILocation(line: 86, column: 5, scope: !239)
!246 = !DILocation(line: 85, column: 34, scope: !240)
!247 = !DILocation(line: 85, column: 28, scope: !240)
!248 = !DILocation(line: 85, column: 21, scope: !240)
!249 = distinct !{!249, !242, !250, !154, !155, !156}
!250 = !DILocation(line: 87, column: 3, scope: !87)
!251 = !DILocation(line: 88, column: 3, scope: !44)
!252 = !DILocation(line: 88, column: 19, scope: !44)
!253 = !DILocation(line: 0, scope: !120, inlinedAt: !254)
!254 = distinct !DILocation(line: 89, column: 3, scope: !44)
!255 = !DILocation(line: 33, column: 3, scope: !120, inlinedAt: !254)
!256 = !DILocation(line: 0, scope: !129, inlinedAt: !254)
!257 = !DILocation(line: 34, column: 3, scope: !129, inlinedAt: !254)
!258 = !DILocation(line: 38, column: 37, scope: !120, inlinedAt: !254)
!259 = !DILocation(line: 38, column: 10, scope: !120, inlinedAt: !254)
!260 = !DILocation(line: 38, column: 3, scope: !120, inlinedAt: !254)
!261 = !DILocation(line: 90, column: 12, scope: !262)
!262 = distinct !DILexicalBlock(scope: !44, file: !45, line: 90, column: 7)
!263 = !DILocation(line: 90, column: 17, scope: !262)
!264 = !DILocation(line: 90, column: 7, scope: !44)
!265 = !DILocation(line: 91, column: 15, scope: !266)
!266 = distinct !DILexicalBlock(scope: !262, file: !45, line: 90, column: 23)
!267 = !DILocation(line: 92, column: 29, scope: !266)
!268 = !DILocation(line: 92, column: 16, scope: !266)
!269 = !DILocation(line: 92, column: 14, scope: !266)
!270 = !DILocation(line: 93, column: 3, scope: !266)
!271 = !DILocation(line: 96, column: 3, scope: !44)
!272 = !DILocation(line: 96, column: 15, scope: !44)
!273 = !DILocation(line: 97, column: 9, scope: !44)
!274 = !DILocation(line: 97, column: 17, scope: !44)
!275 = !{!276, !113, i64 0}
!276 = !{!"User", !113, i64 0, !109, i64 8, !110, i64 24, !113, i64 32}
!277 = !DILocation(line: 98, column: 23, scope: !44)
!278 = !DILocation(line: 98, column: 28, scope: !44)
!279 = !{!276, !110, i64 8}
!280 = !DILocation(line: 99, column: 23, scope: !44)
!281 = !DILocation(line: 99, column: 27, scope: !44)
!282 = !{!276, !113, i64 16}
!283 = !DILocation(line: 100, column: 9, scope: !44)
!284 = !DILocation(line: 100, column: 20, scope: !44)
!285 = !{!276, !113, i64 32}
!286 = !DILocation(line: 101, column: 3, scope: !44)
!287 = !DILocation(line: 101, column: 18, scope: !44)
!288 = !DILocation(line: 102, column: 12, scope: !44)
!289 = !DILocation(line: 102, column: 23, scope: !44)
!290 = !{!291, !113, i64 0}
!291 = !{!"Profile", !113, i64 0, !110, i64 8}
!292 = !DILocation(line: 103, column: 12, scope: !44)
!293 = !DILocation(line: 103, column: 18, scope: !44)
!294 = !{!291, !110, i64 8}
!295 = !DILocation(line: 104, column: 9, scope: !44)
!296 = !DILocation(line: 104, column: 17, scope: !44)
!297 = !{!276, !110, i64 24}
!298 = !DILocation(line: 106, column: 3, scope: !44)
!299 = !DILocation(line: 106, column: 15, scope: !44)
!300 = !DILocation(line: 0, scope: !120, inlinedAt: !301)
!301 = distinct !DILocation(line: 107, column: 3, scope: !44)
!302 = !DILocation(line: 33, column: 3, scope: !120, inlinedAt: !301)
!303 = !DILocation(line: 0, scope: !129, inlinedAt: !301)
!304 = !DILocation(line: 34, column: 3, scope: !129, inlinedAt: !301)
!305 = !DILocation(line: 38, column: 37, scope: !120, inlinedAt: !301)
!306 = !DILocation(line: 38, column: 10, scope: !120, inlinedAt: !301)
!307 = !DILocation(line: 38, column: 3, scope: !120, inlinedAt: !301)
!308 = !DILocation(line: 109, column: 1, scope: !44)
!309 = !DILocation(line: 108, column: 3, scope: !44)
