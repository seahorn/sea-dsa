# SeaHorn builder image that builds binary SeaHorn release package
# Primarily used by the CI
# Arguments:
#  - BUILD_TYPE: Debug, RelWithDebInfo, Coverage
FROM ghcr.io/seahorn/buildpack-deps-seahorn:jammy-llvm18

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
  -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_C_COMPILER=clang-18 \
  -DSEA_ENABLE_LLD=ON \
  -DCPACK_GENERATOR="TGZ" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

RUN cmake --build . --target install

ENV PATH="/sea-dsa/build/run/bin:$PATH"

# lit is provided by the base image, but OutputCheck (which the lit RUN
# directives pipe into) is not; install it.
RUN pip3 install --no-cache-dir OutputCheck

# run tests when they are ready to go
RUN cmake --build . --target test-sea-dsa
RUN cmake --build . --target sea-dsa-units


WORKDIR /sea-dsa
