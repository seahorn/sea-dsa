// clang-format off

// @COMPILE-CMD: $CLANG -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone $THIS -o $OUTPUT

// clang-format on

int add(int a, int b){
    return a + b;
}

int sub(int a, int b){
    return a - b;
}

int use(int (*fn)(int, int)) {
  /// EXPECTED: after one bottom-up pass, main can resolve this
  /// indirect call by replacing it with {&add, &sub}
  int res = fn(10, 20); 
  return res;
}

int main(){
  int x = use(add); 
  int y = use(sub); 
  return x+y;
} 
