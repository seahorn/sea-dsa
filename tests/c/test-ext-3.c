// clang-format off

// clang -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone test-ext-3.c -o ../test-ext-3.ll

// clang-format on

#include "sea_dsa/sea_dsa.h"
#include <stdlib.h>

/*Example of how to use sea_dsa_set_* attribute setters */

extern void print(int *x);
extern unsigned nd_uint(void);

int main(void) {

  // TODO: Use sea_dsa_new();

  {
    // Test multiple attributes set
    int *p;
    sea_dsa_set_ptrtoint(p);
    sea_dsa_set_modified(p);
    sea_dsa_set_read(p);
  }

  {
    // Test sea_dsa_set_ptrtoint
    int *p;
    sea_dsa_set_ptrtoint(p);
  }

  {
    // Test sea_dsa_set_modified
    int *p;
    sea_dsa_set_modified(p);
  }

  {
    // Test sea_dsa_set_read
    int *p;
    sea_dsa_set_read(p);
  }
  return 0;
}