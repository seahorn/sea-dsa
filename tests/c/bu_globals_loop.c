// clang-format off

// @COMPILE-CMD: $CLANG $THIS -S -emit-llvm -O1 -Xclang -disable-llvm-optzns -o $OUTPUT_DIR/bu_globals_loop.ll
// @COMPILE-CMD: $OPT $OUTPUT_DIR/bu_globals_loop.ll -S -mem2reg -simplifycfg -instnamer -o $OUTPUT

// clang-format on

static void *A;
static void *B;
static void *C;

extern void print(void *);

void foo(int x) {
	void **c = x ? &A : &B;
	*c = &C;
	C = &A;
}

void entry(int x) {
	foo(x);
	// x should point back to the node with <A, B>
	void *a = A; 
	print(a);
}
