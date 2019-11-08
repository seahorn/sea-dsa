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
