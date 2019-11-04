#include "sea_dsa/sea_dsa.h"
#include <stdlib.h>

/* Tests for sea_dsa_mk_seq */

extern void print(int* x);
extern unsigned nd_uint(void);

int main(void) {

  {
    int* s1 = (int*) malloc(sizeof(int)*2);

    s1[0] = 0;
    s1[1] = 0;
    
    // GOOD USE of sea_dsa_mk_seq:
    // - the node pointed by s1 is not a sequence and the new size (8) is
    // greater or equal than the current size of the node which is also 8
    sea_dsa_mk_seq(s1, 8);
    
    print(s1);
  }
  {
    int* s1 = (int*) malloc(sizeof(int)*2);

    // sea-dsa marks already the node pointed by s1 as a sequence
    unsigned i = nd_uint();
    s1[i] = 0;
    
    // BAD USE of sea_dsa_mk_seq:
    // - the node pointed by s1 is already a sequence node.
    sea_dsa_mk_seq(s1, 8);
    
    print(s1);
  }
  {
    int* s1 = (int*) malloc(sizeof(int)*2);

    s1[0] = 0;
    s1[1] = 0;
    
    // BAD USE of sea_dsa_mk_seq:
    // - the size of the node pointed by s1 is bigger than 4.
    sea_dsa_mk_seq(s1, 4);
    
    print(s1);
  }
  return 0;
}
