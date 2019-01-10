#
# Dockerfile for SeaHorn binary
# produces package in /seahorn/build
# Arguments:
#  - UBUNTU:     trusty, xenial
#  - BUILD_TYPE: debug, release
#  - TRAVIS:     true (optional, for use on Travis only)
#

ARG UBUNTU

# Pull base image.
FROM seahorn/seahorn-build-llvm5:$UBUNTU

ARG TRAVIS

# clone SeaHorn (just copy on Travis)
# since there are no conditionals in docker,
# always copy, and, if needed, remove and clone instead
COPY . /sea-dsa
RUN if [ "$TRAVIS" != "true" ] ; \
      then cd / && rm -rf /sea-dsa && git clone https://github.com/seahorn/sea-dsa -b llvm-5.0 --depth=10 ; \
    fi && \
    mkdir -p /sea-dsa/build
WORKDIR /sea-dsa/build

ARG BUILD_TYPE
# Build configuration.
RUN cmake -GNinja \
          -DCMAKE_BUILD_TYPE=$BUILD_TYPE \ 
          -DBOOST_ROOT=/deps/boost \
          -DLLVM_DIR=/deps/LLVM-5.0.2-Linux/lib/cmake/llvm \
          -DCMAKE_INSTALL_PREFIX=run \
          -DCMAKE_CXX_COMPILER=g++-5 \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
          ../ && \
    cmake --build . --target install && \
    ln -s /clang-5.0/bin/clang run/bin/clang && \
    ln -s /clang-5.0/bin/clang++ run/bin/clang++

# RUN tests when they are ready to go
RUN cmake --build . --target test-sea-dsa
RUN cmake --build . --target sea-dsa-units

WORKDIR /sea-dsa
