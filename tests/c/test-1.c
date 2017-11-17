extern int nd(void);

void f ( int *x , int *y ) {
  *x = 1 ; *y = 2 ;
}

void g (int *p , int *q , int *r , int *s) {
  f (p,q) ;
  f (r,s);
}

int main (int argc, char**argv){

  int x;
  int y;
  int w;
  int z;

  int *a = &x;
  int *b = &y;
  if (nd ()) a = b;
  
  g(a, b, &w, &z);
  return x + y + w + z;
}
