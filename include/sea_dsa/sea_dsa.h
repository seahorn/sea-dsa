#ifndef _SEADSA__H_
#define _SEADSA__H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
  
// sea-dsa will unify all argument's cells
extern void sea_dsa_alias(const void *p, ...);
// sea-dsa will collapse the argument's cell
extern void sea_dsa_collapse(const void *p);
// sea-dsa will return a fresh memory object
extern void* sea_dsa_new();
// sea-dsa will mark the node pointed by p as a sequence node of size sz
// The noded pointed by p cannot be already a sequence node and its
// size must be less or equal than sz.
extern void sea_dsa_mk_seq(const void *p, unsigned sz);  
#ifdef __cplusplus
}
#endif
#endif 
