#include <stdbool.h>

// clang-format off

// @COMPILE-CMD: $CLANG -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone $THIS -o $OUTPUT

// clang-format on

#define CAS(x, y, z) __atomic_compare_exchange_n(x, &(y), z, true, 0, 0)

extern void __VERIFIER_assert(bool);

int main(void) {
  int *x = 0;
  int y = 0;
  int *z = x;
  CAS(&z, x, &y); // if (z == x) z = &y;
   __VERIFIER_assert(*z == y);
  return 0;
}
