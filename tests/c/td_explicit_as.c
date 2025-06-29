// clang-format off

// @COMPILE-CMD: $CLANG -O0 -c -emit-llvm -S -Xclang -disable-O0-optnone $THIS -o $OUTPUT

// clang-format on

extern void print(const char *);

void foo(const char *s) {
	print(s);
}

void entry(const char *s) {
	print(s);
}

static const char str1[] = "str1";

int main(int argc, char **argv) {
	 // let str1 and str2 have the same local node
	const char *s = argc > 2 ? str1 : "str2";
	foo(s);

	// pass str1 explicitly to entry.
	// inside entry, str1 and str2 should have 2 separate cells
	entry(str1);

	return 0;
}
