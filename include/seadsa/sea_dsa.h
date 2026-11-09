// SPDX-FileCopyrightText: 2026 The sea-dsa Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef _SEADSA__H_
#define _SEADSA__H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// sea-dsa will unify all argument's cells
extern void sea_dsa_alias(const void *p, ...);
// sea-dsa will mark the node pointed to by p as read (R)
extern void sea_dsa_set_read(const void *p);
// sea-dsa will mark the node pointed to by p as modified (M)
extern void sea_dsa_set_modified(const void *p);
// sea-dsa will mark the node pointed to by p as heap (H)
extern void sea_dsa_set_heap(const void *p);
// sea-dsa will mark the node pointed to by p as ptr to int (2)
extern void sea_dsa_set_ptrtoint(const void *p);
// sea-dsa will mark the node pointed to by p as ptr to int (P)
extern void sea_dsa_set_inttoptr(const void *p);
// sea-dsa will mark the node pointed to by p as heap memory (H)
extern void sea_dsa_set_heap(const void *p);
// sea-dsa will mark the node pointed to by p as external (E)
extern void sea_dsa_set_external(const void *p);
// sea-dsa will collapse the argument's cell
extern void sea_dsa_collapse(const void *p);
// sea-dsa will return a fresh memory object
extern void *sea_dsa_new(void) __attribute__((malloc));
// like sea_dsa_new except used for creating a node representation of an
// existing memory object
extern void *sea_dsa_mk(void);
// links one cell to another cell. Can be manually extended with
// sea_dsa_link_<type>(void *p, unsigned offset, <type>> *p2) in order to
// describe what type it is linking to
extern void sea_dsa_link(const void *p, unsigned offset, const void *p2);
// Access annotations record a type at a byte offset relative to p (including
// when p points inside an object). They do not mark memory read or modified.
// Offsets used to be node-absolute; callers using interior pointers must now
// supply offsets relative to that pointer. sea_dsa_link uses the same rule.
// The legacy unsuffixed hook records the pointer argument's type.
extern void sea_dsa_access(const void *p, unsigned offset);
// Declare additional two-argument hooks as needed: sea_dsa_access_<type>.
// Types: iN (positive LLVM integer width), half, bfloat, float, double,
// fp128, x86_fp80, ppc_fp128, ptr (address space 0), pN (address space N).
// N in pN is an unsigned 24-bit address space. Unknown/malformed hooks are
// ignored. Annotate structs, arrays and vectors field by field.
extern void sea_dsa_access_i32(const void *p, unsigned offset);
extern void sea_dsa_access_double(const void *p, unsigned offset);
extern void sea_dsa_access_ptr(const void *p, unsigned offset);
// C examples (int is 32 bits on this target):
//   int x; sea_dsa_access_i32(&x, 0);               // scalar
//   void *q; sea_dsa_access_ptr(&q, 0);            // pointer
//   struct S { int f; double d; void *q; };
//   struct S *s = ...;
//   sea_dsa_access_i32(&s->f, 0);                  // interior pointer
//   sea_dsa_access_i32(s, offsetof(struct S, f));  // struct per field
//   sea_dsa_access_double(s, offsetof(struct S, d));
//   sea_dsa_access_ptr(s, offsetof(struct S, q));
// Include <stddef.h> for offsetof. The named type describes memory, so use
// i8 for a one-byte C _Bool object even if values lower to LLVM i1.

// sea-dsa will mark the node pointed by p as a sequence node of size sz
// The noded pointed by p cannot be already a sequence node and its
// size must be less or equal than sz.
extern void sea_dsa_mk_seq(const void *p, unsigned sz);
#ifdef __cplusplus
}
#endif
#endif
