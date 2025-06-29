// clang-format off

// @COMPILE-CMD: $CLANG -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone $THIS -o $OUTPUT

// clang-format on

#include <string.h>
#include "include/spec_func.h"
#include "sea_dsa.h"
#include <stdint.h>
#include <limits.h>

char *spec_func(const char *s, int c)
{
	sea_dsa_collapse(s);
	sea_dsa_set_read(s);
	sea_dsa_set_modified(s);
	return (char *)s;
}
