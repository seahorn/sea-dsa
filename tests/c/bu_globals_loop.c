// clang-5.0 bu_globals_loop.c -S -emit-llvm -O1 -Xclang -disable-llvm-optzns -o bu_globals_loop.ll
// opt-5.0 bu_globals_loop.ll -S -mem2reg -simplifycfg -instnamer -o bu_globals_loop.opt.ll

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
