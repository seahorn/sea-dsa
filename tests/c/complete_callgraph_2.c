struct class_t;
typedef int (*FN_PTR)(struct class_t *, int);
typedef struct class_t {
  FN_PTR m_foo;
  FN_PTR m_bar;
} class_t;

int foo(class_t *self, int x)
{
  if (x > 10) {
    /// EXPECTED: after one bottom-up pass, main can resolve this
    /// indirect call by replacing it with {&bar}
    return self->m_bar(self, x + 1);
  } else
    return x;
}

int bar (class_t *self, int y) {
  if (y < 100)
    return y + 10;
  else
    return y - 5;
}



int main(void) {
  class_t obj;
  obj.m_foo = &foo;
  obj.m_bar = &bar;

  int res;  
  res = foo(&obj, 42);
  return 0;
}
