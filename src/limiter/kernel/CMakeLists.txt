add_library(aif_limiter_kernels STATIC dispatch.cpp scalar.cpp highway.cpp diagnostic.cpp)
add_library(AvsFilters::LimiterKernels ALIAS aif_limiter_kernels)
set_target_properties(aif_limiter_kernels PROPERTIES EXPORT_NAME LimiterKernels)
target_include_directories(aif_limiter_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_limiter_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_limiter_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_limiter_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_limiter_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_limiter_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_limiter_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/limiter/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/limiter)
endif()
