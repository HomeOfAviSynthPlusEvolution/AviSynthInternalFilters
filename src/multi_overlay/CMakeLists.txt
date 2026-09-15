

if(AIF_BUILD_FILTERS)
  add_library(aif_multi_overlay_filters STATIC filters.cpp multi_overlay.cpp)
  add_library(AvsFilters::MultiOverlayFilters ALIAS aif_multi_overlay_filters)
  set_target_properties(aif_multi_overlay_filters PROPERTIES EXPORT_NAME MultiOverlayFilters)
  target_include_directories(aif_multi_overlay_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_multi_overlay_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_multi_overlay_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/multi_overlay/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/multi_overlay)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_multi_overlay_plugin MODULE plugin.cpp)
  target_include_directories(aif_multi_overlay_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_multi_overlay_plugin PRIVATE AvsFilters::MultiOverlayFilters)
  set_target_properties(aif_multi_overlay_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_multi_overlay")
  aif_target_defaults(aif_multi_overlay_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_multi_overlay_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_multi_overlay_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
