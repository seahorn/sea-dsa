#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "seahorn/seahorn.h"

typedef int(*handler_t)();


struct test_handler {
  handler_t handler;
};

int foo() {
  uint32_t i = 0;
  return i;
}

struct test_handler handle_var = {
     .handler = foo
};

struct test_handler *return_handler_struct () {
  return &handle_var;
}

int main(void) {
  sassert(return_handler_struct()->handler() == 0);
  return 0;
}


