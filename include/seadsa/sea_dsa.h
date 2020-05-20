#ifndef _SEADSA__H_
#define _SEADSA__H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
  
// sea-dsa will unify all argument's cells
extern void seadsa_alias(const void *p, ...);
// sea-dsa will mark the node pointed to by p as read (R)
extern void sea_dsa_read(const void *p);
// sea-dsa will mark the node pointed to by p as modified (M)
extern void sea_dsa_modified(const void *p);
// sea-dsa will mark the node pointed to by p as ptr to int (2)
extern void sea_dsa_ptrtoint(const void *p);
// sea-dsa will collapse the argument's cell
extern void seadsa_collapse(const void *p);
// sea-dsa will return a fresh memory object
extern void* seadsa_new();
// sea-dsa will mark the node pointed by p as a sequence node of size sz
// The noded pointed by p cannot be already a sequence node and its
// size must be less or equal than sz.
extern void seadsa_mk_seq(const void *p, unsigned sz);
#ifdef __cplusplus
}
#endif
#endif 
