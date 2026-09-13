add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_convolution_filters STATIC filters.cpp general_convolution.cpp)
  add_library(AvsFilters::ConvolutionFilters ALIAS aif_convolution_filters)
  set_target_properties(aif_convolution_filters PROPERTIES EXPORT_NAME ConvolutionFilters)
  target_link_libraries(aif_convolution_filters PUBLIC AvsFilters::ConvolutionKernels)
  target_include_directories(aif_convolution_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_convolution_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_convolution_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/convolution/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/convolution)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_convolution_plugin MODULE plugin.cpp)
  target_include_directories(aif_convolution_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_convolution_plugin PRIVATE AvsFilters::ConvolutionFilters)
  set_target_properties(aif_convolution_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_convolution")
  aif_target_defaults(aif_convolution_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_convolution_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_convolution_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
