// Paper same-node unification example (curl if2ip pattern): one pointer
// may address two different fields of the same object.
extern int nd_int(void);
struct s { int tag; int a4; int a8; int a12; };
int main(void) {
  struct s x = {0, 1, 2, 3};
  int *p = nd_int() ? &x.a4 : &x.a8;
  *p = 5;
  return x.tag + x.a12;
}
