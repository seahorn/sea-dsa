; RUN: %seadsa %s --sea-dsa-callgraph-dot --sea-dsa-dot-outdir=%T/complete_callgraph_unprototyped.ll
; RUN: %cmp-graphs %tests/complete_callgraph_unprototyped.dot %T/complete_callgraph_unprototyped.ll/callgraph.dot | OutputCheck %s -d --comment=";"
; CHECK: ^OK$

; A call to an unprototyped function, as clang emits it: a direct call whose
; signature (i32 ()) differs from the callee's (i32 (...)).
; CallBase::getCalledFunction() returns null on that mismatch, so
; llvm::CallGraph records no edge and sea-dsa has to add it back.
;
; Both calls must show up as main -> nd_uint edges: without them the graph
; has main and nd_uint as unconnected nodes, and the bottom-up/top-down
; analyses visit them in an order that ignores the dependency.

target datalayout = "e-m:o-i64:64-i128:128-n32:64-S128"

define i32 @main() {
  %1 = call i32 @nd_uint()
  %2 = call i32 @nd_uint()
  %3 = add nsw i32 %1, %2
  ret i32 %3
}

declare i32 @nd_uint(...)
