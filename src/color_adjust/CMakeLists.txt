add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_color_adjust_filters STATIC filters.cpp color_yuv.cpp levels.cpp rgb_adjust.cpp tweak.cpp)
  add_library(AvsFilters::ColorAdjustFilters ALIAS aif_color_adjust_filters)
  set_target_properties(aif_color_adjust_filters PROPERTIES EXPORT_NAME ColorAdjustFilters)
  target_link_libraries(aif_color_adjust_filters PUBLIC AvsFilters::ColorAdjustKernels)
  target_include_directories(aif_color_adjust_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_color_adjust_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_color_adjust_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/color_adjust/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/color_adjust)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_color_adjust_plugin MODULE plugin.cpp)
  target_include_directories(aif_color_adjust_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_color_adjust_plugin PRIVATE AvsFilters::ColorAdjustFilters)
  set_target_properties(aif_color_adjust_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_color_adjust")
  aif_target_defaults(aif_color_adjust_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_color_adjust_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_color_adjust_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
