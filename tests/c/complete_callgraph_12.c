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
typedef struct {
  int* x;
  binfptr fun;
} Struct;

Struct arith_functions[4] = { {0,add}, {0,sub}, {0,mul}, {0,div}};

extern unsigned nd_uint();

int main(){

  int val = 789;
  arith_functions[0].x = &val;
  arith_functions[1].x = &val;
  arith_functions[2].x = &val;
  arith_functions[3].x = &val; 
  
  int res = 0;
  int x = nd_uint();
  int y = nd_uint();
  switch(nd_uint()) {
  case 0:
    res = *(arith_functions[0].x) + arith_functions[0].fun(x,y);
    break;
  case 1:
    res = *(arith_functions[1].x) + arith_functions[1].fun(x,y);
    break;
  case 2:
    res = *(arith_functions[2].x) + arith_functions[2].fun(x,y);
    break;
  case 3:
    res = *(arith_functions[3].x) + arith_functions[3].fun(x,y);
    break;
  default:;;
  }
  return res;
} 
