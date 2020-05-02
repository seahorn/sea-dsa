# SeaHorn builder image that builds binary SeaHorn release package
# Primarily used by the CI
# Arguments:
#  - BUILD_TYPE: Debug, RelWithDebInfo, Coverage
FROM seahorn/buildpack-deps-seahorn:bionic-llvm10


# Assume that docker-build is ran in the top-level SeaHorn directory
COPY . /sea-dsa
# Re-create the build directory that might have been present in the source tree
RUN rm -rf /sea-dsa/build /sea-dsa/debug /sea-dsa/release && \
  mkdir /sea-dsa/build

WORKDIR /sea-dsa/build

ARG BUILD_TYPE=RelWithDebInfo

# Build configuration
RUN cmake .. -GNinja \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCMAKE_INSTALL_PREFIX=run \
  -DCMAKE_CXX_COMPILER=clang++-10 \
  -DCMAKE_C_COMPILER=clang-10 \
  -DSEA_ENABLE_LLD=ON \
  -DCPACK_GENERATOR="TGZ" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

RUN cmake --build . --target install

RUN ln -s /usr/local/bin/lit bin/llvm-lit
ENV PATH "/sea-dsa/build/run/bin:$PATH"

# run tests when they are ready to go
RUN cmake --build . --target test-sea-dsa
RUN cmake --build . --target sea-dsa-units


WORKDIR /sea-dsa
