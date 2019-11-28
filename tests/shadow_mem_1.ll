; RUN: %seadsa -sea-dsa-shadow-mem %cs_dsa %s -oll %s.inst.ll --outdir=output
; OutputCheck --file-to-check=output/%s.inst.ll %s -d --comment=";"

; CHECK-L: %sm3 = call i32 @shadow.mem.arg.init(i32 0, i8* null), !shadow.mem !3, !shadow.mem.def !3
; CHECK-L: call void @shadow.mem.out(i32 0, i32 %shadow.mem.0.1, i32 1, i8* null), !shadow.mem !3, !shadow.mem.use !3
; CHECK-L: %sm3 = call i32 @shadow.mem.init(i32 40, i8* null), !shadow.mem !3, !shadow.mem.def !3
; CHECK-L: %sm5 = call i32 @shadow.mem.init(i32 24, i8* null), !shadow.mem !3, !shadow.mem.def !3
; CHECK-L: %sh = call i32 @shadow.mem.arg.new(i32 24, i32 %sm5, i32 0, i8* null), !shadow.mem !3, !shadow.mem.def !3
; CHECK-L: %sh2 = call i32 @shadow.mem.arg.new(i32 40, i32 %sm3, i32 0, i8* null), !shadow.mem !3, !shadow.mem.def !3

; ModuleID = 'shadow_mem_1.c'
source_filename = "shadow_mem_1.c"
target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.14.0"

%struct.node = type { %struct.node*, %struct.element* }
%struct.element = type { i32, i32 }

; Function Attrs: nounwind ssp uwtable
define %struct.node* @mkList(i32, %struct.element*) local_unnamed_addr #0 {
  %3 = icmp slt i32 %0, 1
  br i1 %3, label %21, label %4

; <label>:4:                                      ; preds = %2
  %5 = tail call i8* @mymalloc(i32 16) #2
  %6 = bitcast i8* %5 to %struct.node*
  %7 = add nsw i32 %0, -1
  br label %8

; <label>:8:                                      ; preds = %4, %15
  %9 = phi i32 [ 0, %4 ], [ %19, %15 ]
  %10 = phi %struct.node* [ %6, %4 ], [ %17, %15 ]
  %11 = getelementptr inbounds %struct.node, %struct.node* %10, i64 0, i32 1
  store %struct.element* %1, %struct.element** %11, align 8, !tbaa !3
  %12 = icmp eq i32 %9, %7
  br i1 %12, label %13, label %15

; <label>:13:                                     ; preds = %8
  %14 = getelementptr inbounds %struct.node, %struct.node* %10, i64 0, i32 0
  store %struct.node* null, %struct.node** %14, align 8, !tbaa !8
  br label %21

; <label>:15:                                     ; preds = %8
  %16 = tail call i8* @mymalloc(i32 16) #2
  %17 = bitcast i8* %16 to %struct.node*
  %18 = bitcast %struct.node* %10 to i8**
  store i8* %16, i8** %18, align 8, !tbaa !8
  %19 = add nuw nsw i32 %9, 1
  %20 = icmp slt i32 %19, %0
  br i1 %20, label %8, label %21

; <label>:21:                                     ; preds = %15, %13, %2
  %22 = phi %struct.node* [ null, %2 ], [ %6, %13 ], [ %6, %15 ]
  ret %struct.node* %22
}

declare i8* @mymalloc(i32) local_unnamed_addr #1

; Function Attrs: nounwind ssp uwtable
define i32 @main() local_unnamed_addr #0 {
  %1 = tail call i8* @mymalloc(i32 8) #2
  %2 = bitcast i8* %1 to %struct.element*
  %3 = bitcast i8* %1 to i32*
  store i32 5, i32* %3, align 4, !tbaa !9
  %4 = getelementptr inbounds i8, i8* %1, i64 4
  %5 = bitcast i8* %4 to i32*
  store i32 6, i32* %5, align 4, !tbaa !12
  %6 = tail call %struct.node* @mkList(i32 5, %struct.element* %2)
  %7 = tail call %struct.node* @mkList(i32 5, %struct.element* %2)
  %8 = icmp eq %struct.node* %6, null
  br i1 %8, label %19, label %9

; <label>:9:                                      ; preds = %0
  br label %10

; <label>:10:                                     ; preds = %9, %10
  %11 = phi %struct.node* [ %17, %10 ], [ %6, %9 ]
  %12 = getelementptr inbounds %struct.node, %struct.node* %11, i64 0, i32 1
  %13 = load %struct.element*, %struct.element** %12, align 8, !tbaa !3
  %14 = getelementptr inbounds %struct.element, %struct.element* %13, i64 0, i32 0
  %15 = load i32, i32* %14, align 4, !tbaa !9
  tail call void @print(i32 %15) #2
  %16 = getelementptr inbounds %struct.node, %struct.node* %11, i64 0, i32 0
  %17 = load %struct.node*, %struct.node** %16, align 8, !tbaa !8
  %18 = icmp eq %struct.node* %17, null
  br i1 %18, label %19, label %10

; <label>:19:                                     ; preds = %10, %0
  %20 = icmp eq %struct.node* %7, null
  br i1 %20, label %31, label %21

; <label>:21:                                     ; preds = %19
  br label %22

; <label>:22:                                     ; preds = %21, %22
  %23 = phi %struct.node* [ %29, %22 ], [ %7, %21 ]
  %24 = getelementptr inbounds %struct.node, %struct.node* %23, i64 0, i32 1
  %25 = load %struct.element*, %struct.element** %24, align 8, !tbaa !3
  %26 = getelementptr inbounds %struct.element, %struct.element* %25, i64 0, i32 1
  %27 = load i32, i32* %26, align 4, !tbaa !12
  tail call void @print(i32 %27) #2
  %28 = getelementptr inbounds %struct.node, %struct.node* %23, i64 0, i32 0
  %29 = load %struct.node*, %struct.node** %28, align 8, !tbaa !8
  %30 = icmp eq %struct.node* %29, null
  br i1 %30, label %31, label %22

; <label>:31:                                     ; preds = %22, %19
  ret i32 0
}

declare void @print(i32) local_unnamed_addr #1

attributes #0 = { nounwind ssp uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind }

!llvm.module.flags = !{!0, !1}
!llvm.ident = !{!2}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{!"clang version 5.0.0 (tags/RELEASE_500/final)"}
!3 = !{!4, !5, i64 8}
!4 = !{!"node", !5, i64 0, !5, i64 8}
!5 = !{!"any pointer", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C/C++ TBAA"}
!8 = !{!4, !5, i64 0}
!9 = !{!10, !11, i64 0}
!10 = !{!"element", !11, i64 0, !11, i64 4}
!11 = !{!"int", !6, i64 0}
!12 = !{!10, !11, i64 4}
