int add(int a, int b){
    return a + b;
}

int sub(int a, int b){
    return a - b;
}

typedef int (*binfptr)(int, int);

extern binfptr nd_binfptr();

int apply(int (*fn)(int, int), int x, int y) {
  return fn(x,y) + nd_binfptr()(x,y);
}
binfptr bar(int (*fn)(int, int)) {
  return fn;
}

int foo(int (*fn)(int, int), int x, int y) {
  int (* res) (int,int)  = bar(fn);
  return apply(res, x,y);
}

int main(){
  int x = foo(&add, 2, 5); 
  int y = foo(&sub, 5, 7); 
  return x+y;
} 
