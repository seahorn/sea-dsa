// clang-format off

// @COMPILE-CMD: $CLANG $THIS -S -emit-llvm -O1 -Xclang -disable-llvm-optzns -o $OUTPUT
// @COMPILE-CMD: $OPT $OUTPUT -S -mem2reg -simplifycfg -instnamer -o bu_affected_globals.opt.ll

// clang-format on

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
