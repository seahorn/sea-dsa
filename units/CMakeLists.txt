enable_testing()
include (CTest)

set (USED_LIBS_SEA_DSA_UNITS
  ${Boost_SYSTEM_LIBRARY}
  SeaDsaAnalysis
  )

set(LLVM_LINK_COMPONENTS irreader bitwriter ipo scalaropts instrumentation core
    # XXX not clear why these last two are required
    codegen objcarcopts)

add_executable(units_sea_dsa EXCLUDE_FROM_ALL
    units_sea_dsa.cpp
    FieldTypeTests.cpp
  )
llvm_config (units_sea_dsa ${LLVM_LINK_COMPONENTS})

target_link_libraries(units_sea_dsa PRIVATE ${USED_LIBS_SEA_DSA_UNITS})
add_custom_target(sea-dsa-units units_sea_dsa DEPENDS units_sea_dsa)
add_test(NAME SeaDsaTests COMMAND units_sea_dsa)
