; RUN: %seadsa %s --sea-dsa=bu --sea-dsa-type-aware --sea-dsa-gen-specs=%t-access.spec.ll --sea-dsa-dot --sea-dsa-dot-outdir=%t-access-spec-original
; RUN: %seadsa %s --sea-dsa=bu --sea-dsa-type-aware --sea-dsa-specfile=%t-access.spec.ll --sea-dsa-dot --sea-dsa-dot-outdir=%t-access-spec-roundtrip
; RUN: cat %t-access-spec-roundtrip/accesses.mem.dot | OutputCheck %s --comment=";"
; CHECK: (?=.*0:ptr)(?=.*12:i32).*:SR

target datalayout = "e-p:64:64-i64:64-n8:16:32:64-S128"

define i32 @accesses(ptr %base) {
  %interior = getelementptr i8, ptr %base, i64 8
  %field = getelementptr i8, ptr %interior, i64 4
  %value = load i32, ptr %field
  %pointer = load ptr, ptr %base
  ret i32 %value
}
