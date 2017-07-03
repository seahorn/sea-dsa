add_definitions(-D__STDC_CONSTANT_MACROS)
add_definitions(-D__STDC_LIMIT_MACROS)

set(LLVM_LINK_COMPONENTS 
  irreader 
  bitwriter
  ipa
  ipo 
  scalaropts 
  instrumentation 
  core 
  codegen 
  objcarcopts)


add_executable(seadsa seadsa.cc)
target_link_libraries (seadsa SeaDsaAnalysis)
llvm_config (seadsa ${LLVM_LINK_COMPONENTS})
install(TARGETS seadsa RUNTIME DESTINATION bin)

if (SEA_DSA_STATIC_EXE)
  set (CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
  set_target_properties (seadsa PROPERTIES LINK_SEARCH_START_STATIC ON)
  set_target_properties (seadsa PROPERTIES LINK_SEARCH_END_STATIC ON)
endif()
