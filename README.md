# TeaDsa: A Points-to Analysis for Verification of Low-level C/C++ #

<a href="https://travis-ci.org/seahorn/sea-dsa">
    <img src="https://travis-ci.org/seahorn/sea-dsa.svg?branch=master" title="Ubuntu 14.04 LTS 64bit, g++-5"/>
</a>


`TeaDsa` is a context-, field-, and array-sensitive unification-based points-to
analysis for LLVM bitcode inspired
by [DSA](http://llvm.org/pubs/2003-11-15-DataStructureAnalysisTR.ps), and based
on [SeaDsa](https://github.com/seahorn/sea-dsa). `TeaDsa` is an order of
magnitude more scalable and precise than `SeaDsa` thanks to improved handling of
context sensitivity, addition of partial flow-sensitivity, and type-awareness.  

Although `TeaDsa` can analyze arbitrary LLVM bitcode, it has been
tailored for use in program verification of C/C++ programs. It can be used as a
stand-alone tool or together with the
[SeaHorn](https://github.com/seahorn/seahorn/tree/tea-dsa) verification
framework and its analyses.

## Requirements ## 

`sea-dsa` is written in C++ and uses the Boost library. The main requirements
are: 

- C++ compiler supporting c++11
- Boost >= 1.55
- LLVM 5.0

To run tests, install the following packages:

- `sudo pip install lit OutputCheck`
- `sudo easy_install networkx`
- `sudo apt-get install libgraphviz-dev`
- `sudo easy_install pygraphviz`

## Project Structure ##
1. The main Points-To Graph data structures, `Graph`, `Cell`, and `Node`, are
   defined in `include/Graph.hh` and `src/Graph.cc`.
2. The *Local* analysis is in `include/Local.hh` and `src/DsaLocal.cc`.
3. The *Bottom-Up* analysis is in `include/BottomUp.hh` and
   `src/DsaBottomUp.cc`.
4. The *Top-Down* analysis is in `include/TopDown.hh` and `src/DsaTopDown.cc`.
5. The interprocedural node cloner is in `include/Cloner.hh` and
   `src/Clonner.cc`.
6. Type handling code is in `include/FieldType.hh`, `include/TypeUtils.hh`, 
   `src/FieldType.cc`, and `src/TypeUtils.cc`.
7. The allocator function discovery is in `include/AllocWrapInfo.hh` and
   `src/AllocWrapInfo.cc`.

## Compilation and Usage ##

### Program Verification benchmarks ###
Instructions on running program verification benchmarks, together with recipes
for building real-world projects and our results, can be found in
[tea-dsa-extras](https://github.com/kuhar/tea-dsa-extras).

### Integration in other C++ projects (for users) ## 

`TeaDsa` contains two directories: `include` and `src`. Since
`TeaDsa` analyzes LLVM bitcode, LLVM header files and libraries must
be accessible when building with `TeaDsa`.

If your project uses `cmake` then you just need to add in your
project's `CMakeLists.txt`:

	 include_directories(sea_dsa/include)
	 add_subdirectory(sea_dsa)

### Standalone (for developers) ###

If you already installed `llvm-5.0` on your machine:

    mkdir build && cd build
	cmake -DCMAKE_INSTALL_PREFIX=run -DLLVM_DIR=__here_llvm-5.0__/share/llvm/cmake  ..
   	cmake --build . --target install
	
Otherwise:

    mkdir build && cd build
	cmake -DCMAKE_INSTALL_PREFIX=run ..
    cmake --build . --target install

To run tests:

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
   Programs" by A. Gurfinkel and J. A. Navas. In SAS'17.
   ([Paper](https://jorgenavas.github.io/papers/sea-dsa-SAS17.pdf))
   | ([Slides](https://jorgenavas.github.io/slides/sea-dsa-SAS17-slides.pdf))



