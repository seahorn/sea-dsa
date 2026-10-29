; RUN: %seadsa %s --sea-dsa-dot --sea-dsa-dot-outdir=%T/memcpy_byte_storage.ll
; RUN: %cmp-graphs %tests/memcpy_byte_storage.entry.mem.dot %T/memcpy_byte_storage.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; RUN: %seadsa %s --sea-dsa-trust-byte-storage --sea-dsa-dot --sea-dsa-dot-outdir=%T/memcpy_byte_storage.trust.ll
; RUN: %cmp-graphs %tests/memcpy_byte_storage.trust.entry.mem.dot %T/memcpy_byte_storage.trust.ll/entry.mem.dot both | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; A memcpy out of untyped byte storage may carry pointers.
;
; rustc (since 1.79) declares every alloca as [N x i8], whatever it holds.
; Here %v holds a Vec-like { ptr, cap, len } whose ptr is a malloc result,
; and moving the Vec copies the 24 bytes into %w. The declared type [24 x i8]
; has no pointer member, but that says nothing about the contents: the cells
; of %v and %w must be unified, so the pointer loaded from %w reaches the
; malloc node. Otherwise the store through %q is invisible at %p.
;
; Two copies transfer no pointers and must leave their operands separate:
; %a -> %b, because [6 x i32] is a real type with no pointer member, and
; @str -> %s, because @str is a constant holding plain bytes (clang's copy of
; a local `char s[] = "aeiou"`).
;
; --sea-dsa-trust-byte-storage restores the old, unsound behavior: the
; [24 x i8] copy is assumed to carry no pointers, so %v/%w and %p/%q stay
; in separate nodes.

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@str = private unnamed_addr constant [6 x i8] c"aeiou\00", align 1

declare noalias ptr @malloc(i64)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg)

define void @entry() {
  %v = alloca [24 x i8], align 8
  %w = alloca [24 x i8], align 8
  %p = call ptr @malloc(i64 4)
  store ptr %p, ptr %v, align 8
  %cap = getelementptr inbounds i8, ptr %v, i64 8
  store i64 1, ptr %cap, align 8
  %len = getelementptr inbounds i8, ptr %v, i64 16
  store i64 1, ptr %len, align 8
  call void @llvm.memcpy.p0.p0.i64(ptr align 8 %w, ptr align 8 %v, i64 24, i1 false)
  %q = load ptr, ptr %w, align 8
  store i32 42, ptr %q, align 4

  %a = alloca [6 x i32], align 4
  %b = alloca [6 x i32], align 4
  store i32 7, ptr %a, align 4
  call void @llvm.memcpy.p0.p0.i64(ptr align 4 %b, ptr align 4 %a, i64 24, i1 false)

  %s = alloca [6 x i8], align 1
  call void @llvm.memcpy.p0.p0.i64(ptr align 1 %s, ptr align 1 @str, i64 6, i1 false)
  ret void
}
