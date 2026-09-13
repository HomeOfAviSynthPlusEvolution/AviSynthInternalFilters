add_library(aif_planes_kernels STATIC dispatch.cpp scalar.cpp highway.cpp)
add_library(AvsFilters::PlanesKernels ALIAS aif_planes_kernels)
set_target_properties(aif_planes_kernels PROPERTIES EXPORT_NAME PlanesKernels)
target_include_directories(aif_planes_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_planes_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_planes_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_planes_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_planes_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_planes_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_planes_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/planes/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/planes)
endif()

target_link_libraries(aif_planes_kernels PRIVATE AvsFilters::BlankClipKernels)
