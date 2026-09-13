add_library(aif_focus_kernels STATIC dispatch.cpp highway.cpp)
add_library(AvsFilters::FocusKernels ALIAS aif_focus_kernels)
set_target_properties(aif_focus_kernels PROPERTIES EXPORT_NAME FocusKernels)
target_include_directories(aif_focus_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_focus_kernels)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" aif_processor)
if(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]")
  set(aif_processor "arm64")
endif()
if(AIF_SCALAR_ONLY)
  target_compile_definitions(aif_focus_kernels PRIVATE AIF_SCALAR_ONLY)
else()
  if(TARGET hwy::hwy)
    target_link_libraries(aif_focus_kernels PRIVATE hwy::hwy)
  else()
    target_link_libraries(aif_focus_kernels PRIVATE hwy)
  endif()
  target_include_directories(aif_focus_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  if(MSVC AND aif_processor MATCHES "^(amd64|x86_64|x86|i[3-6]86)$")
    target_sources(aif_focus_kernels PRIVATE highway_avx2.cpp)
    target_compile_definitions(aif_focus_kernels PRIVATE AIF_FOCUS_SEPARATE_AVX2)
    set_source_files_properties(highway_avx2.cpp PROPERTIES COMPILE_OPTIONS /arch:AVX2)
  endif()
endif()

if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_focus_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/focus/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/focus)
endif()

