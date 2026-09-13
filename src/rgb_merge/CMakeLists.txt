add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_rgb_merge_filters STATIC filters.cpp merge_rgb.cpp)
  add_library(AvsFilters::RgbMergeFilters ALIAS aif_rgb_merge_filters)
  set_target_properties(aif_rgb_merge_filters PROPERTIES EXPORT_NAME RgbMergeFilters)
  target_link_libraries(aif_rgb_merge_filters PUBLIC AvsFilters::RgbMergeKernels)
  target_include_directories(aif_rgb_merge_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_rgb_merge_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_rgb_merge_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/rgb_merge/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rgb_merge)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_rgb_merge_plugin MODULE plugin.cpp)
  target_include_directories(aif_rgb_merge_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_rgb_merge_plugin PRIVATE AvsFilters::RgbMergeFilters)
  set_target_properties(aif_rgb_merge_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_rgb_merge")
  aif_target_defaults(aif_rgb_merge_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_rgb_merge_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_rgb_merge_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
