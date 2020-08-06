add_definitions(-D__STDC_CONSTANT_MACROS)
add_definitions(-D__STDC_LIMIT_MACROS)
find_program(CLANG-FORMAT NAMES clang-format clang-format-mp-10 clang-format-10 REQUIRED)

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

message(${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_C_COMPILER_ID MATCHES Clang)
  add_custom_command(
    OUTPUT libc.spec.c
    COMMAND libcSpecGen
    COMMAND ${CLANG-FORMAT} -i libc.spec.c
    DEPENDS libcSpecGen
    )

  add_library(LibcSpec-64 OBJECT libc.spec.c)
  target_compile_options(LibcSpec-64 BEFORE PRIVATE -emit-llvm)
  install(FILES $<TARGET_OBJECTS:LibcSpec-64> 
    DESTINATION lib 
    RENAME libc-64.spec.bc
    )  

  option(SEAHORN_BUILD_32_BIT_SPEC "Build 32bit library specs" OFF)
  if(SEAHORN_BUILD_32_BIT_SPEC) 

    add_executable(LibcSpec-32 OBJECT libc.spec.c)
    target_compile_options(LibcSpec-32 BEFORE PRIVATE -m32 -emit-llvm)
    install(FILES $<TARGET_OBJECTS:LibcSpec-32> 
      DESTINATION lib 
      RENAME libc-32.spec.bc
      )

  endif()
else()
  message(WARNING "Unable to build sea-dsa spec files using current compiler!\n\
\t Set C compiler to clang using -DCMAKE_C_COMPILER=clang\n\
\t Current C compiler is: ${CMAKE_C_COMPILER_ID}")
endif()
