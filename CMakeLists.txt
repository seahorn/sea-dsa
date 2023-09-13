cmake_minimum_required(VERSION 3.10.2)
project(sea-dsa)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR )
  message (FATAL_ERROR
    "In-source builds are not allowed. Please clean your source tree and try again.")
endif()

# determine if this is top-level or embedded project
if (PROJECT_NAME STREQUAL CMAKE_PROJECT_NAME)
  set (TOP_LEVEL TRUE)

else()
  set (TOP_LEVEL FALSE)
endif()



if(TOP_LEVEL)
  if(CMAKE_VERSION VERSION_GREATER "3.13.0")
    cmake_policy(SET CMP0074 NEW)
    cmake_policy(SET CMP0077 NEW)
  endif()

  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
  endif()
endif()

option (BUILD_SEA_DSA_LIBS_SHARED "Build all sea-dsa libraries shared." OFF)
option (ENABLE_SANITY_CHECKS "Enable sea-dsa sanity checks." OFF)

if (ENABLE_SANITY_CHECKS)
  message (STATUS "Compiling sea-dsa with sanity checks")
  set (SANITY_CHECKS TRUE)
else ()
  message (STATUS "Compiling sea-dsa without sanity checks")
endif()

# Add path for custom modules
list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# put static libraries first
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})

option(SEA_ENABLE_LLD "Use lld as C and C++ linker." OFF)
if(SEA_ENABLE_LLD)
  set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
endif()


option (SEA_DSA_STATIC_EXE "Static executable." OFF)

## llvm
if (TOP_LEVEL)
  find_package (LLVM 14.0 CONFIG)
  if (NOT LLVM_FOUND)
    ExternalProject_Get_Property (llvm INSTALL_DIR)
    set (LLVM_ROOT ${INSTALL_DIR})
    set (LLVM_DIR ${LLVM_ROOT}/lib/cmake/llvm CACHE PATH
      "Forced location of LLVM cmake config" FORCE)
    message (WARNING "No llvm found. Install LLVM 14.")
    return()
  endif()

  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  # We incorporate the CMake features provided by LLVM:
  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
  include(AddLLVM)
  include(HandleLLVMOptions)
  set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin)
  set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/lib)

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_CXXFLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LLVM_LDFLAGS}")

  include_directories(${LLVM_INCLUDE_DIRS})
  link_directories(${LLVM_LIBRARY_DIRS})
  add_definitions(${LLVM_DEFINITIONS})

  if (NOT LLVM_BUILD_TYPE STREQUAL CMAKE_BUILD_TYPE)
    message(WARNING
      "LLVM_BUILD_TYPE and CMAKE_BUILD_TYPE differ:\n"
      "\tLLMV_BUILD_TYPE=${LLVM_BUILD_TYPE}\n"
      "\tCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}\n"
      "Runtime errors might occur.")
    if (LLVM_BUILD_TYPE MATCHES "Release|RelWithDebInfo" AND
        CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
      message(STATUS "Assuming that mixing Release and RelWithDebInfo is allowed.")
    else()
      message(FATAL_ERROR "Incompatible mix of LLVM_BUILD_TYPE and CMAKE_BUILD_TYPE")
    endif()
  endif()
endif ()


## Boost
if (TOP_LEVEL)
  set (CUSTOM_BOOST_ROOT "" CACHE PATH "Path to custom boost installation.")
  if (CUSTOM_BOOST_ROOT)
    set (BOOST_ROOT ${CUSTOM_BOOST_ROOT})
    set (Boost_NO_SYSTEM_PATHS "ON")
  endif()


  find_package (Boost 1.65 REQUIRED)
  if (Boost_FOUND)
    include_directories (${Boost_INCLUDE_DIRS})
    if(NOT LLVM_ENABLE_EH)
      add_definitions(-DBOOST_NO_EXCEPTIONS)
    endif()
  endif ()
endif ()

# sea-dsa specific
set(CMAKE_CXX_STANDARD 14 CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)

add_definitions(-Wno-redeclared-class-member)
add_definitions(-Wno-sometimes-uninitialized)
add_definitions(-Wno-deprecated-declarations)
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  add_definitions( -Wno-unused-local-typedefs)
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  add_definitions(-Wno-unused-local-typedef)
  add_definitions(-Wno-inconsistent-missing-override)
endif ()

configure_file(include/seadsa/config.h.cmake
  ${CMAKE_BINARY_DIR}/include/seadsa/config.h)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(lib/seadsa)
add_subdirectory(units)
add_subdirectory(tools)
add_subdirectory(tests)

install(DIRECTORY include/seadsa DESTINATION include
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.hh"
  PATTERN "*.h"
  PATTERN "CMakeFiles" EXCLUDE
  PATTERN ".svn" EXCLUDE)

if (TOP_LEVEL)
  install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION .)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  add_library(SpecLang OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/lib/seadsa/SpecLang.cc)
  target_compile_options(SpecLang BEFORE PRIVATE -S -emit-llvm)
  install(FILES $<TARGET_OBJECTS:SpecLang>
    DESTINATION lib
    RENAME sea_dsa.ll)
else()
  message (WARNING "Unable to build spec language \n\t Set C compiler to clang with -DCMAKE_C_COMPILER=clang")
endif()
