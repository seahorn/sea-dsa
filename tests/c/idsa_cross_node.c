// Cross-node unification with delta alignment: cells of two distinct
// nodes at different offsets are unified through one pointer.
#include <stdlib.h>
extern int nd_int(void);
struct A { int f0; int f4; };
struct B { int g0; int g4; int g8; };
int main(void) {
  struct A *a = (struct A *)malloc(sizeof(struct A));
  struct B *b = (struct B *)malloc(sizeof(struct B));
  int *p = nd_int() ? &a->f4 : &b->g8;
  *p = 7;
  return a->f0 + b->g0;
}
