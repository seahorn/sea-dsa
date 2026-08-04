; RUN: %seadsa %s %ci_dsa --sea-dsa-type-aware=true --sea-dsa-dot --sea-dsa-dot-outdir=%T/gep_multidim.ll
; RUN: cat %T/gep_multidim.ll/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=ROW1
; RUN: cat %T/gep_multidim.ll/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=ROW5
; RUN: cat %T/gep_multidim.ll/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=STRUCT
; RUN: cat %T/gep_multidim.ll/main.mem.dot | OutputCheck %s -d --comment=";" --check-prefix=NOSTALE

; The first GEP index strides over the source element type, it does not index
; into it. Descending into the source element type before applying index 0
; consumes that index twice, which only shows when the source element type is
; an aggregate: multi-dimensional arrays and arrays of structs.

; @table: source element type [2 x i32] (8 bytes), so t[1][1] is at byte 12 and
; t[5][1] at byte 44. The doubly-consumed first index would give 8 and 24.
; ROW1: 12:i32
; ROW5: 44:i32

; @arr: source element type [4 x %struct.S] (32 bytes), so s[1][2].snd is at
; 32 + 16 + 4 = 52. The doubly-consumed first index would give 8 + 16 + 4 = 28.
; STRUCT: 52:i32

; NOSTALE-NOT: (^|[^0-9])(8|24|28):i32

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"

%struct.S = type { i32, i32 }

@table = global [6 x [2 x i32]] zeroinitializer, align 4
@arr = global [2 x [4 x %struct.S]] zeroinitializer, align 4

; multi-dimensional array: t[1][1] and t[5][1]
define internal i32 @f([2 x i32]* %t) {
entry:
  %p1 = getelementptr inbounds [2 x i32], [2 x i32]* %t, i64 1, i64 1
  %v1 = load i32, i32* %p1, align 4
  %p5 = getelementptr inbounds [2 x i32], [2 x i32]* %t, i64 5, i64 1
  %v5 = load i32, i32* %p5, align 4
  %add = add i32 %v1, %v5
  ret i32 %add
}

; array of structs: s[1][2].snd
define internal i32 @g([4 x %struct.S]* %s) {
entry:
  %q = getelementptr inbounds [4 x %struct.S], [4 x %struct.S]* %s, i64 1, i64 2, i32 1
  %v = load i32, i32* %q, align 4
  ret i32 %v
}

define i32 @main() {
entry:
  %t = getelementptr inbounds [6 x [2 x i32]], [6 x [2 x i32]]* @table, i64 0, i64 0
  %a = getelementptr inbounds [2 x [4 x %struct.S]], [2 x [4 x %struct.S]]* @arr, i64 0, i64 0
  %r1 = call i32 @f([2 x i32]* %t)
  %r2 = call i32 @g([4 x %struct.S]* %a)
  %r = add i32 %r1, %r2
  ret i32 %r
}
