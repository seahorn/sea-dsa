; Indirect call through a function pointer set by a GLOBAL initializer
; (@g = global ptr @foo). sea-dsa resolves the call to @foo because
; DsaLocal.cc BlockBuilderBase::init() records the initializer constant as an
; allocation site, which is what CompleteCallGraphAnalysis matches against
; Function to find the callees.
;
; RUN: %seadsa %s %cs_dsa --sea-dsa-callgraph-stats 2>&1 | OutputCheck %s -d --comment=";"
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
