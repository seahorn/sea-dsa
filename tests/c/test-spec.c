// clang-format off

// clang -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone test-spec.c -o ../test-spec.ll

// clang-format on


#include <stdio.h>
#include <string.h>
#include "include/spec_func.h"

char *test_prop(const char *a, char b)
{
	return spec_func(a, b); 
}


int main() {

	const char vowel[] = "aeiou";
	const char v = 'i';
	const char c = 'b';
	char* retS;
	char* retF;

	retS = test_prop(vowel, v);
	retF = spec_func(vowel, c);

	printf("success: %s \nfail: %s\n", retS, retF);

	return (0);
}
