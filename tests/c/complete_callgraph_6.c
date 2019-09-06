int add(int a, int b){
    return a + b;
}

int sub(int a, int b){
    return a - b;
}

int mul(int a, int b){
    return a * b;
}

int use(int (*fn)(int, int)) {
  /// EXPECTED: after one bottom-up pass, main can resolve this
  /// indirect call by replacing it with {&add, &sub, &mul}
  int res = fn(10, 20); 
  return res;
}

int f1(){
  int x = use(add); 
  int y = use(sub); 
  return x+y;
} 

int f2(){
  int x = use(mul); 
  int y = use(mul); 
  return x+y;
} 


int main () {
  return f1() + f2();
}
