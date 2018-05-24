# sea-dsa: A Heap Analysis for Verification of Low-level C/C++ #

<a href="https://travis-ci.org/seahorn/sea-dsa"><img src="https://travis-ci.org/seahorn/sea-dsa.svg?branch=master" title="Ubuntu 12.04 LTS 64bit, g++-4.8"/></a>


`sea-dsa` is a context-, field-, and array-sensitive heap analysis for
LLVM bitcode inspired
by [DSA](http://llvm.org/pubs/2003-11-15-DataStructureAnalysisTR.ps).

Although `sea-dsa` can analyze arbitrary LLVM bitcode, it has been
designed with the only purpose of assisting logic-based verification
tools that target C/C++ programs. The goal of `sea-dsa` is to produce
a finer-grained partition of the heap so that verifiers can generate
verification conditions involving complex low-level pointers and
arrays constraints but yet they can be solved efficiently.

## Requirements ## 

`sea-dsa` is written in C++ and uses heavily the Boost library. The
main requirements are:

- C++ compiler supporting c++11
- Boost >= 1.55
- LLVM 5.0

To run tests, install the following packages:

- `sudo pip install lit OutputCheck`
- `sudo easy_install networkx`
- `sudo apt-get install libgraphviz-dev`
- `sudo easy_install pygraphviz`

## Compilation and Usage ##

### Integration in other C++ projects (for users) ## 

`sea-dsa` contains two directories: `include` and `src`. Since
`sea-dsa` analyzes LLVM bitcode, LLVM header files and libraries must
be accessible when building with `sea-dsa`.

If your project uses `cmake` then you just need to add in your
project's `CMakeLists.txt`:

	 include_directories (sea_dsa/include)
	 add_subdirectory(sea_dsa)

### Standalone (for developers) ###

If you already installed `llvm-5.0` in your machine:

    mkdir build && cd build
	cmake -DCMAKE_INSTALL_PREFIX=__dir__ -DLLVM_DIR=__here_llvm-5.0__/share/llvm/cmake  ..
   	cmake --build . --target install
	cmake --build . --target test-sea-dsa
	
Otherwise:

    mkdir build && cd build
	cmake -DCMAKE_INSTALL_PREFIX=__dir__ ..
    cmake --build . --target install

To run tests:

    export SEADSA=__dir__/bin
	cmake --build . --target test-sea-dsa

## Dealing with C/C++ library and external calls ##

The pointer semantics of external calls can be defined by writing a
wrapper that calls any of these two functions (defined in
`sea_dsa/sea_dsa.h`):

- `extern void sea_dsa_alias(const void *p, ...);`
- `extern void sea_dsa_collapse(const void *p);`

The first function unifies all argument's cells while the second one
tells `sea-dsa` to collapse the argument's cell.

For instance, consider an external call `foo` defined as follows:

	`extern void* foo(const void*p1, void *p2, void *p3);`

Suppose that the returned pointer should be unified to `p2` but not to
`p1`. In addition, we would like to collapse the cell corresponding to
`p3`. Then, we can replace the above prototype of `foo` with the
following definition:

	#include "sea_dsa/sea_dsa.h"
	void* foo(const void*p1, void *p2, void*p3) {
		void* r = sea_dsa_new();
		sea_dsa_alias(r,p2);
		sea_dsa_collapse(p3);
		return r;
	}

where `sea_dsa_new`, `sea_dsa_alias`, and `sea_dsa_collapse` are
defined in `sea_dsa/sea_dsa.h`.

## References ## 

1. "A Context-Sensitive Memory Model for Verification of C/C++
   Programs" by A. Gurfinkel and J. A. Navas. In SAS'17. ([Paper](https://jorgenavas.github.io/papers/sea-dsa-SAS17.pdf)) | ([Slides](https://jorgenavas.github.io/slides/sea-dsa-SAS17-slides.pdf))



