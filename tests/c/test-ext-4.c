// clang-format off

// clang -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone test-ext-4.c -o ../test-ext-4.ll

// clang-format on

#include "sea_dsa.h"

/* Tests for sea_dsa_new */

int main(void) {

  {
    // Test no assignment
    sea_dsa_new();
  }

  {
    // Test direct use
    sea_dsa_set_heap(sea_dsa_new());
  }

  {
    // Test assignment and attribute setting
    void *p = sea_dsa_new();
    sea_dsa_set_heap(p);
  }

  {
    // Test use later
    void *(*func)()  = &sea_dsa_new;
    void *p = (*func)();
    sea_dsa_set_heap(p);
  }
  return 0;
}