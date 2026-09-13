add_library(aif_rgb_merge_kernels STATIC dispatch.cpp)
add_library(AvsFilters::RgbMergeKernels ALIAS aif_rgb_merge_kernels)
set_target_properties(aif_rgb_merge_kernels PROPERTIES EXPORT_NAME RgbMergeKernels)
target_include_directories(aif_rgb_merge_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_rgb_merge_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_rgb_merge_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_rgb_merge_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_rgb_merge_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_rgb_merge_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_rgb_merge_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/rgb_merge/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rgb_merge)
endif()

target_link_libraries(aif_rgb_merge_kernels PRIVATE AviSynth::ConvertVideo)
