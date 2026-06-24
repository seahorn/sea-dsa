; Indirect call through a function pointer set by a GLOBAL initializer
; (@g = global ptr @foo). sea-dsa should resolve the call to @foo, but under
; LLVM-15 opaque pointers the global-initializer linking in
; DsaLocal.cc BlockBuilderBase::init() is disabled (the old
; getElementType()->isFunctionTy() check is unavailable on opaque pointers), so
; the indirect call is left UNRESOLVED. Equivalent code that sets the pointer
; with a runtime store is resolved correctly.
;
; XFAIL until global-initializer function-pointer linking is restored; the
; CHECK below asserts the desired (resolved) behavior.
;
; RUN: %seadsa %s %cs_dsa --sea-dsa-callgraph-stats 2>&1 | OutputCheck %s -d --comment=";"
; XFAIL: *
; CHECK: indirect calls resolved by Sea-Dsa 1

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

@g = dso_local global ptr @foo, align 8

define dso_local i32 @foo() {
  ret i32 0
}

define dso_local i32 @main() {
  %1 = load ptr, ptr @g, align 8
  %2 = call i32 (...) %1()
  ret i32 %2
}
