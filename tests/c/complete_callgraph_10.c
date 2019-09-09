int add(int a, int b){
  return a + b;
}

int sub(int a, int b){
  return a - b;
}

int mul(int a, int b){
  return a*b;   
}

int div(int a, int b){
  return a/b;   
}

typedef int (*binfptr)(int, int);

binfptr arith_functions[4] = { add, sub, mul, div};

extern unsigned nd_uint();

int main(){

  int res = 0;
  int x = nd_uint();
  int y = nd_uint();
  switch(nd_uint()) {
  case 0:
    res = arith_functions[0](x,y);
    break;
  case 1:
    res = arith_functions[1](x,y);
    break;
  case 2:
    res = arith_functions[2](x,y);
    break;
  case 3:
    res = arith_functions[3](x,y);
    break;
  default:;;
  }
  return res;
} 
