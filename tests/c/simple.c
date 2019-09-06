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
