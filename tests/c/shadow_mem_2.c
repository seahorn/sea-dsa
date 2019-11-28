extern int nd(void);
extern void __VERIFIER_error(void) __attribute__((noreturn));
extern void __VERIFIER_assume(int);
#define assert(X)                                                              \
  if (!(X)) {                                                                  \
    __VERIFIER_error();                                                        \
  }
#define assume __VERIFIER_assume

extern int *freshInt(void);
extern void sea_dsa_alias(int *, int *);

int main() {
  int *a = freshInt();
  int *b = freshInt();

  sea_dsa_alias(a, b);

  a[0] = 33;
  a[1] = 44;
  b[0] = 52;

  assert(a[0] + a[1] == 96);
  return 0;
}

