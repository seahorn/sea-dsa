// Degeneration: joining a cell at offset 0 with an unbounded interval
// yields [0,+oo) — nothing outside the interval remains, so partial
// collapse soundly degenerates to a full offset collapse.
#include <stdlib.h>
extern int nd_int(void);
extern char nd_char(void);
struct rb { int id; int size; char buf[]; };
int main(void) {
  int n = nd_int();
  if (n <= 1) return 0;
  struct rb *b = (struct rb *)malloc(sizeof(struct rb) + n);
  b->id = 1;
  b->size = n;
  for (int i = 0; i < n; ++i) b->buf[i] = nd_char();
  char *p = nd_int() ? (char *)&b->id : &b->buf[0];
  *p = nd_char();
  return b->id;
}
