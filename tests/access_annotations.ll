; RUN: %seadsa %s --sea-dsa=cs --sea-dsa-dot --sea-dsa-dot-outdir=%t-access-default
; RUN: cat %t-access-default/seed.mem.dot | OutputCheck %s --comment=";" --check-prefix=SEED
; RUN: %seadsa %s --sea-dsa=cs --sea-dsa-type-aware --sea-dsa-dot --sea-dsa-dot-outdir=%t-access-aware
; RUN: cat %t-access-aware/seed.mem.dot | OutputCheck %s --comment=";" --check-prefix=SEED
; RUN: cat %t-access-aware/invalid.mem.dot | OutputCheck %s --comment=";" --check-prefix=INVALID
; RUN: cat %t-access-aware/address_space.mem.dot | OutputCheck %s --comment=";" --check-prefix=AS
; SEED: (?=.*0:i16)(?=.*12:i32)(?=.*24:double)(?=.*32:ptr)(?=.*40:i64)(?=.*48:i8).*:S}
; INVALID: label=.*void.*:S}
; AS: (?=.*0:ptr addrspace\(1\))(?=.*8:ptr addrspace\(1\))(?=.*16:ptr[,}])(?!.*24:).*:S}

target datalayout = "e-p:64:64-p1:64:64-i64:64-n8:16:32:64-S128"

declare void @sea_dsa_access_i16(ptr, i32)
declare void @sea_dsa_access_i32(ptr, i32)
declare void @sea_dsa_access(ptr, i32)
declare void @sea_dsa_access_double(ptr, i32)
declare void @sea_dsa_access_i64(ptr, i32)
declare void @sea_dsa_access_i8(ptr, i32)

define void @seed(ptr %base) {
  %interior = getelementptr i8, ptr %base, i64 8
  call void @sea_dsa_access_i16(ptr %base, i32 0)
  call void @sea_dsa_access_i32(ptr %interior, i32 4)
  call void @sea_dsa_access_double(ptr %interior, i32 16)
  call void @sea_dsa_access(ptr %base, i32 32)
  call void @sea_dsa_access_i64(ptr %base, i32 40)
  call void @sea_dsa_access_i8(ptr %base, i32 48)
  ret void
}

declare void @sea_dsa_access_named_struct(ptr, i32)
declare void @sea_dsa_access_i0(ptr, i32)
declare void @sea_dsa_access_i32junk(ptr, i32)
declare void @sea_dsa_access_i99999999999999999(ptr, i32)
declare void @sea_dsa_access_type_missing(ptr, i32)
declare void @sea_dsa_access_type_old(ptr, i32, double)

define void @invalid(ptr %base, i32 %offset) {
  call void @sea_dsa_access_named_struct(ptr %base, i32 0)
  call void @sea_dsa_access_i0(ptr %base, i32 0)
  call void @sea_dsa_access_i32junk(ptr %base, i32 0)
  call void @sea_dsa_access_i99999999999999999(ptr %base, i32 0)
  call void @sea_dsa_access_type_missing(ptr %base, i32 0)
  call void @sea_dsa_access_type_old(ptr %base, i32 0, double 0.0)
  call void @sea_dsa_access_i32(ptr %base, i32 %offset)
  ret void
}

declare void @sea_dsa_access_p1(ptr addrspace(1), i32)
declare void @sea_dsa_access_as_legacy(ptr addrspace(1), i32) ; unsupported suffix
; Opaque-pointer calls may use the legacy function through a different signature.
declare void @sea_dsa_access_ptr(ptr addrspace(1), i32)

define void @address_space(ptr addrspace(1) %base) {
  call void @sea_dsa_access_p1(ptr addrspace(1) %base, i32 0)
  call void @sea_dsa_access(ptr addrspace(1) %base, i32 8)
  call void @sea_dsa_access_ptr(ptr addrspace(1) %base, i32 16)
  call void @sea_dsa_access_as_legacy(ptr addrspace(1) %base, i32 24)
  ret void
}
