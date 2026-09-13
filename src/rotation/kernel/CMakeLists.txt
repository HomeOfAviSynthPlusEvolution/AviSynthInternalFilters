add_library(aif_rotation_kernels STATIC dispatch.cpp scalar.cpp highway.cpp)
add_library(AvsFilters::RotationKernels ALIAS aif_rotation_kernels)
set_target_properties(aif_rotation_kernels PROPERTIES EXPORT_NAME RotationKernels)
target_include_directories(aif_rotation_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_rotation_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_rotation_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_rotation_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_rotation_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_rotation_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_rotation_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/rotation/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rotation)
endif()
