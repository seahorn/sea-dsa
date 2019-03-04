extern void print(const char *);

static const char str1[] = "str1";
static const char str2[] = "str2";

const char *foo(int x) {
	// str1 and str2 are in the same local equivalence class
	const char *c = x ? str1 : str2;
	print(c);

	// explicitly return str1
	return str1;
}

int main(int argc, char **argv) {
	// c always points to str1
	const char *c = foo(argc);
	print(c);
	return 0;
}
