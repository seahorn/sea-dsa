#include "seahorn/seahorn.h"
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
typedef int (*handler_t)();
#define containerof(ptr, type, member) \
    ((type *)((uintptr_t)(ptr) - offsetof(type, member)))
struct test_sub{
    int miscellaneous;
};
struct test_handler{
    struct test_sub common;
    handler_t handler;
};
static struct test_handler* to_test_handler(
        struct test_sub* sub) {
    sassert(sub);
    return containerof(sub, struct test_handler, common);
}
int foo() {
    int i = 0;
    sassert(i == 0);
    return i;
}
struct test_handler *return_handler_struct() {
    struct test_handler* handler_ptr = malloc(sizeof(struct test_handler));
    handler_ptr->handler = foo;
    return handler_ptr;
}
int main(int argc, char** argv) {
    struct test_handler *res = return_handler_struct();
    struct test_sub *sub = res;
    struct test_handler *cast_ptr = to_test_handler(sub); // indirect pointer cast
    sassert(cast_ptr->handler() == 0);
    return 0;
}
