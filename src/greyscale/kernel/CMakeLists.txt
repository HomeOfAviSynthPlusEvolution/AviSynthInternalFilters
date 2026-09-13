add_library(aif_greyscale_kernels STATIC dispatch.cpp scalar.cpp highway.cpp)
add_library(AvsFilters::GreyscaleKernels ALIAS aif_greyscale_kernels)
set_target_properties(aif_greyscale_kernels PROPERTIES EXPORT_NAME GreyscaleKernels)
target_include_directories(aif_greyscale_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_greyscale_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_greyscale_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_greyscale_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_greyscale_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_greyscale_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_greyscale_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/greyscale/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/greyscale)
endif()

target_link_libraries(aif_greyscale_kernels PRIVATE AviSynth::ConvertVideo)
