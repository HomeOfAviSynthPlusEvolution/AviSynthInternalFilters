add_library(aif_convolution_kernels STATIC dispatch.cpp scalar.cpp highway.cpp)
add_library(AvsFilters::ConvolutionKernels ALIAS aif_convolution_kernels)
set_target_properties(aif_convolution_kernels PROPERTIES EXPORT_NAME ConvolutionKernels)
target_include_directories(aif_convolution_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_convolution_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_convolution_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_convolution_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_convolution_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_convolution_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_convolution_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/convolution/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/convolution)
endif()

if(MSVC)
 target_compile_options(aif_convolution_kernels PRIVATE /fp:strict)
else()
 target_compile_options(aif_convolution_kernels PRIVATE -ffp-contract=off)
endif()
