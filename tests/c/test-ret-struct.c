// clang-format off

// @COMPILE-CMD: $CLANG -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone $THIS -o $OUTPUT

// clang-format on

#include <assert.h> 

int* A;

typedef struct {
  int* a;
  long b;
} S;

S foo() {
  S x = {A, 2L};
  assert(1);
  return x;
}

int main() {
  int a = 1;
  A = &a;
  S y = foo();
  assert(*(y.a) == 1);
  return 0;
}
