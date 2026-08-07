# find lit. Assume we are using external lit installed from pip
find_program(LIT_COMMAND lit)

# add_lit_testsuite() runs LLVM_EXTERNAL_LIT, not LIT_COMMAND. LLVMConfig.cmake
# exports LLVM_EXTERNAL_LIT as the llvm-lit from LLVM's own build tree, which a
# packaged LLVM does not ship -- Debian's llvm-16 has no
# /usr/lib/llvm-16/bin/llvm-lit. add_lit_testsuite() then warns and silently
# falls back to a llvm-lit inside our build tree that nothing creates, so the
# test target only works if something symlinks one into place. Use the lit we
# just found instead. An LLVM_EXTERNAL_LIT that does exist is left alone, so
# -DLLVM_EXTERNAL_LIT=... still wins.
#
# This has to go in the cache: get_llvm_lit_path() creates the cache entry
# itself, and creating a cache entry drops any normal variable of the same name,
# so setting a normal variable here would be silently discarded.
if (LIT_COMMAND AND NOT EXISTS "${LLVM_EXTERNAL_LIT}")
  message(STATUS "No usable LLVM_EXTERNAL_LIT, using ${LIT_COMMAND}")
  set(LLVM_EXTERNAL_LIT "${LIT_COMMAND}" CACHE STRING "Command used to spawn lit" FORCE)
endif()

if (TOP_LEVEL OR TARGET sea-dsa-tool)
add_lit_testsuite(test-sea-dsa "Regression test for sea dsa"
  -v
  ${CMAKE_CURRENT_SOURCE_DIR}
  PARAMS
  test_dir=${CMAKE_CURRENT_BINARY_DIR}
  ARGS
  --path=${CMAKE_INSTALL_PREFIX}/bin
  DEPENDS seadsa install
  )
endif()
