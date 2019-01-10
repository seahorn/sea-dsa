# find lit. Assume we are using external lit installed from pip
find_program(LIT_COMMAND lit)

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
