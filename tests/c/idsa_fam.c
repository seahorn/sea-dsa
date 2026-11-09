// Paper overview example (redis repl_block pattern): a struct with scalar
// fields and a trailing flexible array member indexed symbolically.
#include <stdlib.h>
struct rb { int id; int size; int used; char buf[]; };
extern int nd_int(void);
extern char nd_char(void);
int g_id = 0;
int main(void) {
  int n = nd_int();
  if (n <= 0) return 0;
  struct rb *b = (struct rb *)malloc(sizeof(struct rb) + n);
  b->id = g_id++;
  b->size = n;
  b->used = 0;
  for (int i = 0; i < n; ++i)
    b->buf[i] = nd_char();
  return b->id;
}
