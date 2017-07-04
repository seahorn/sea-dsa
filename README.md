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
- LLVM 3.6

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

If you already installed `llvm-3.6` in your machine:

    mkdir build && cd build
	cmake -DCMAKE_INSTALL_PREFIX=__dir__ -DLLVM_DIR=__here_llvm-3.6__/share/llvm/cmake  ..
    cmake --build . --target install

Otherwise:

    mkdir build && cd build
	cmake -DCMAKE_INSTALL_PREFIX=__dir__ ..
    cmake --build . --target install

To run tests:

    export SEADSA=__dir__/bin
	cmake --build . --target test-dsa

## References ## 

1. "A Context-Sensitive Memory Model for Verification of C/C++
   Programs" by A. Gurfinkel and J. A. Navas. In SAS'17.



