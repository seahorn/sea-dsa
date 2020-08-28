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
extern void *sea_dsa_new() __attribute__((malloc));
// like sea_dsa_new except used for creating a node representation of an
// existing memory object
extern void *sea_dsa_mk();
// links one cell to another cell. Can be manually extended with
// sea_dsa_link_<type>(void *p, unsigned offset, <type>> *p2) in order to
// describe what type it is linking to
extern void sea_dsa_link(const void *p, unsigned offset, const void *p2);
// access type of p. Can manually extend with
// sea_dsa_access_<type>(<type> *p, unsigned offset) in order to describe the
// type p accesses at an offset
extern void sea_dsa_access(const void *p, unsigned offset);
// sea-dsa will mark the node pointed by p as a sequence node of size sz
// The noded pointed by p cannot be already a sequence node and its
// size must be less or equal than sz.
extern void sea_dsa_mk_seq(const void *p, unsigned sz);
#ifdef __cplusplus
}
#endif
#endif
