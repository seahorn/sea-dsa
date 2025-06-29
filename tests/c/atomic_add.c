#include <stdbool.h>
#include <stdatomic.h>

// clang-format off

// @COMPILE-CMD: $CLANG -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone $THIS -o $OUTPUT

// clang-format on

#define Atomic_Add(ptr, val) __atomic_fetch_add(ptr, val, memory_order_relaxed)

extern void __VERIFIER_assert(bool);

int main(void) {

  int x = 5;
  int *ptr = &x;
  int y = 7;
  Atomic_Add(ptr, y);
  
  __VERIFIER_assert(*ptr == 7);
  return 0;
}
