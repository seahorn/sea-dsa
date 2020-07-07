add_definitions(-D__STDC_CONSTANT_MACROS)
add_definitions(-D__STDC_LIMIT_MACROS)
find_program(CLANG-FORMAT NAMES clang-format clang-format-mp-10 clang-format-10 REQUIRED)


set(SPEC_DIR ${CMAKE_CURRENT_BINARY_DIR}/specs)

set(LLVM_LINK_COMPONENTS
  irreader
  bitwriter
  analysis
  ipo
  scalaropts
  instrumentation
  core
  codegen
  objcarcopts)


add_llvm_executable(seadsa DISABLE_LLVM_LINK_LLVM_DYLIB seadsa.cc)
add_executable(libcSpecGen libcSpecGen.cc)

target_link_libraries (seadsa PRIVATE SeaDsaAnalysis)
add_custom_target(sea-dsa-tool DEPENDS seadsa)


if (TOP_LEVEL OR TARGET sea-dsa-tool)
  install(TARGETS seadsa RUNTIME DESTINATION bin)
endif()

if (SEA_DSA_STATIC_EXE)
  set (CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
  set_target_properties (seadsa PROPERTIES LINK_SEARCH_START_STATIC ON)
  set_target_properties (seadsa PROPERTIES LINK_SEARCH_END_STATIC ON)
endif()

file(
  MAKE_DIRECTORY
  ${SPEC_DIR}
)

add_custom_command(
  OUTPUT ${SPEC_DIR}/libc.spec.c 
  DEPENDS libcSpecGen 
  COMMAND libcSpecGen
  COMMAND ${CLANG-FORMAT} -i ${SPEC_DIR}/libc.spec.c
)

add_library(LibcSpecs OBJECT ${SPEC_DIR}/libc.spec.c)
set_target_properties(LibcSpecs PROPERTIES 
  COMPILE_FLAGS "-emit-llvm" LINK_FLAGS "-emit-llvm")

 install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/LibcSpecs.dir/specs/libc.spec.c.o
   DESTINATION lib
   RENAME libc.spec.bc)

option(SEAHORN_BUILD_32_BIT_SPEC "Build 32bit library specs" OFF)
if(SEAHORN_BUILD_32_BIT_SPEC) 
  add_library(LibcSpecs-32 OBJECT ${SPEC_DIR}/libc.spec.c)
  set_target_properties(LibcSpecs-32 PROPERTIES
    COMPILE_FLAGS "-emit-llvm -m32" LINK_FLAGS "-emit-llvm -m32")

    install (FILES  ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/LibcSpecs-32.dir/specs/libc.spec.c.o
    DESTINATION lib
    RENAME libc-32.spec.bc)

endif()