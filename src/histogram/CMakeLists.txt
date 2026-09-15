

if(AIF_BUILD_FILTERS)
  add_library(aif_histogram_filters STATIC filters.cpp histogram.cpp support.cpp kernel/render.cpp)
  add_library(AvsFilters::HistogramFilters ALIAS aif_histogram_filters)
  set_target_properties(aif_histogram_filters PROPERTIES EXPORT_NAME HistogramFilters)
  target_include_directories(aif_histogram_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  aif_target_defaults(aif_histogram_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_histogram_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/histogram/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/histogram)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_histogram_plugin MODULE plugin.cpp)
  target_include_directories(aif_histogram_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(aif_histogram_plugin PRIVATE AvsFilters::HistogramFilters)
  set_target_properties(aif_histogram_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_histogram")
  aif_target_defaults(aif_histogram_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_histogram_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_histogram_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
