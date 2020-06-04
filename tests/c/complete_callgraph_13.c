// Similar to complete_callgraph_9.c

int add(int a, int b){
    return a + b;
}

int sub(int a, int b){
    return a - b;
}

typedef int (*binfptr)(int, int);

extern binfptr nd_binfptr();

int apply(int (*fn)(int, int), int x, int y) {
  // We don't resolve because possible callee is the pointer returned
  // by external call nd_binfptr
  return fn(x,y);
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
  //int y = foo(&sub, 5, 7);
  int y = foo(nd_binfptr(), 5, 7);
  return x+y;
} 
