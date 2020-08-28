#include "seadsa/sea_dsa.h"

int main() {
  void *p = sea_dsa_new();
  sea_dsa_collapse(p);
  sea_dsa_set_heap(p);
  sea_dsa_set_read(p);
  sea_dsa_set_modified(p);
  sea_dsa_set_ptrtoint(p);
  sea_dsa_set_inttoptr(p);
  sea_dsa_set_external(p);
  sea_dsa_alias(p);
  sea_dsa_link(p, 0, p);
  sea_dsa_access(p, 0);
  void *a = sea_dsa_mk();
  return 0;
}