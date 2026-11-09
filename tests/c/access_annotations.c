#include "seadsa/sea_dsa.h"
#include <stddef.h>
struct S {
  int pad;
  int f;
  double d;
  void *q;
};
extern void sea_dsa_access_p1(const void *, unsigned);
void c_access(struct S *s, void *target) {
  sea_dsa_access_i32(&s->f, 0);
  sea_dsa_access_double(&s->d, 0);
  sea_dsa_access_ptr(&s->q, 0);
  sea_dsa_access_i32(s, offsetof(struct S, pad));
  sea_dsa_access_i32(s, offsetof(struct S, f));
  sea_dsa_access_double(s, offsetof(struct S, d));
  sea_dsa_access_ptr(s, offsetof(struct S, q));
  sea_dsa_link(&s->q, 0, target);
}
typedef void __attribute__((address_space(1))) * as_pointer;
struct AS {
  double pad;
  as_pointer q;
};
void c_address_space(struct AS *p) { sea_dsa_access_p1(&p->q, 0); }
