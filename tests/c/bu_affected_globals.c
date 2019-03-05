// clang-5.0 bu_affected_globals.c -S -emit-llvm -O1 -Xclang -disable-llvm-optzns -o bu_affected_globals.ll
// opt-5.0 bu_affected_globals.ll -S -mem2reg -simplifycfg -instnamer -o bu_affected_globals.opt.ll

static void *storage;

static void *confuse;

extern void print(void *);

void foo() {
	// storage is unreachable from returns and formals,
	// but the modification should be carried over back
	// to the caller `entry`.
	storage = malloc(64);

	// even though `storage` can be confused locally
	// with `confuse`, the local confusion shouldn't be
	// poropagated to `entry`, as it's not having any side
	// effects outside of `foo`. IE, confuse shouldn't
	// point to the malloced cell.
	void *c = storage ? storage : confuse;
	print(c);
}

void entry() {
	foo();
	// x should point to the malloced cell for `storage` only.
	void *x = storage; 
	print(x);
}
