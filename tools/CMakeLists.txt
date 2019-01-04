add_definitions(-D__STDC_CONSTANT_MACROS)
add_definitions(-D__STDC_LIMIT_MACROS)

set(LLVM_LINK_COMPONENTS 
  irreader 
  bitwriter
  ipo 
  scalaropts 
  instrumentation 
  core 
  codegen 
  objcarcopts)


add_llvm_executable(seadsa seadsa.cc)
target_link_libraries (seadsa SeaDsaAnalysis)
llvm_config (seadsa ${LLVM_LINK_COMPONENTS})
add_custom_target(sea-dsa-tool DEPENDS seadsa)

if (TOP_LEVEL OR TARGET sea-dsa-tool)
  install(TARGETS seadsa RUNTIME DESTINATION bin)
endif()

if (SEA_DSA_STATIC_EXE)
  set (CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
  set_target_properties (seadsa PROPERTIES LINK_SEARCH_START_STATIC ON)
  set_target_properties (seadsa PROPERTIES LINK_SEARCH_END_STATIC ON)
endif()
