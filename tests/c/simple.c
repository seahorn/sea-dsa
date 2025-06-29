// clang-format off

// @COMPILE-CMD: $CLANG -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone $THIS -o $OUTPUT

// clang-format on

#include <stdlib.h>

typedef struct S {
  int** x;
  int** y;  
} S;

int g;

int main(int argc, char** argv){
  S s1, s2;
  int* p1 = (int*) malloc(sizeof(int));
  int* q1 = (int*) malloc(sizeof(int));  
  s1.x = &p1;
  s1.y = &q1;    
  *(s1.x) = &g;
  return 0;
}   
