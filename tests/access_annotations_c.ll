; RUN: clang-18 -S -emit-llvm -O1 -I%S/../include %S/c/access_annotations.c -o %t.ll
; RUN: %seadsa %t.ll --sea-dsa=cs --sea-dsa-type-aware --sea-dsa-dot --sea-dsa-dot-outdir=%t-c
; RUN: cat %t-c/c_access.mem.dot | OutputCheck %s --comment=";" --check-prefix=C
; RUN: cat %t-c/c_access.mem.dot | OutputCheck %s --comment=";" --check-prefix=LINK
; RUN: cat %t-c/c_address_space.mem.dot | OutputCheck %s --comment=";" --check-prefix=AS
; C: (?=.*0:i32)(?=.*4:i32)(?=.*8:double)(?=.*16:ptr).*:S
; LINK: <16, ptr
; AS: 8:ptr addrspace\(1\)
