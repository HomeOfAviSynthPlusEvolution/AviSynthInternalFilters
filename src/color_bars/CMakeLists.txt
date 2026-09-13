add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_color_bars_filters STATIC filters.cpp color_bars.cpp single_frame.cpp)
  add_library(AvsFilters::ColorBarsFilters ALIAS aif_color_bars_filters)
  set_target_properties(aif_color_bars_filters PROPERTIES EXPORT_NAME ColorBarsFilters)
  target_link_libraries(aif_color_bars_filters PUBLIC AvsFilters::ColorBarsKernels)
  target_include_directories(aif_color_bars_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_color_bars_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_color_bars_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/color_bars/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/color_bars)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_color_bars_plugin MODULE plugin.cpp)
  target_include_directories(aif_color_bars_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_color_bars_plugin PRIVATE AvsFilters::ColorBarsFilters)
  set_target_properties(aif_color_bars_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_color_bars")
  aif_target_defaults(aif_color_bars_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_color_bars_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_color_bars_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
