#include <stdlib.h>

int jenkins_hash(int x) {
  return x;
}

int MurmurHash3_hash(int x) {
  return x;
}

extern int nd();

typedef int (*fptr)(int);
fptr hash; 
fptr* hash_ptr = &hash;

void hash_init() {
  if (nd()) {
    *hash_ptr = &jenkins_hash;
  } else {
    *hash_ptr = &MurmurHash3_hash;
  }
}


typedef struct  Struct {
  int x;
  int y;
  fptr* hashF;
} Struct;


void init(Struct* s, void (*hash_init_f)(void)) {
  hash_init_f();
  // *(s->hashF) = *hash_ptr;
  *(s->hashF) = hash;
}
int main(){
  Struct* s = (Struct*) malloc(sizeof(Struct));
  s->x = 0;
  s->y = 0;
  s->hashF = (fptr*)malloc(sizeof(fptr));
  init(s, &hash_init);
  return (*(s->hashF))(8);
} 
