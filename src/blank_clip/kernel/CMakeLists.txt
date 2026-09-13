add_library(aif_blank_clip_kernels STATIC dispatch.cpp scalar.cpp highway.cpp)
add_library(AvsFilters::BlankClipKernels ALIAS aif_blank_clip_kernels)
set_target_properties(aif_blank_clip_kernels PROPERTIES EXPORT_NAME BlankClipKernels)
target_include_directories(aif_blank_clip_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_blank_clip_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_blank_clip_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_blank_clip_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_blank_clip_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_blank_clip_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_blank_clip_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/blank_clip/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/blank_clip)
endif()
