// Similar to complete_callgraph_9.c

int add(int a, int b){
    return a + b;
}

int sub(int a, int b){
    return a - b;
}

typedef int (*binfptr)(int, int);

// this adds the noalias attribute which indicates that there is an
// allocation site inside the function
extern binfptr nd_binfptr(void) __attribute__((malloc));

int apply(int (*fn)(int, int), int x, int y) {
  // Possibly callees for fn are &add and the return value of
  // nd_binfptr.
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
  int y = foo(nd_binfptr(), 5, 7);
  return x+y;
} 
