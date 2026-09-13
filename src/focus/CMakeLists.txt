add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_focus_filters STATIC
    filters.cpp
    blur_sharpen.cpp
    adjust_focus_h.cpp
    adjust_focus_v.cpp
    temporal_soften.cpp
    spatial_soften.cpp
  )
  add_library(AvsFilters::FocusFilters ALIAS aif_focus_filters)
  set_target_properties(aif_focus_filters PROPERTIES EXPORT_NAME FocusFilters)
  target_link_libraries(aif_focus_filters PUBLIC AvsFilters::FocusKernels)
  target_include_directories(aif_focus_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_focus_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_focus_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/focus/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/focus)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_focus_plugin MODULE plugin.cpp)
  target_include_directories(aif_focus_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_focus_plugin PRIVATE AvsFilters::FocusFilters)
  set_target_properties(aif_focus_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_focus")
  aif_target_defaults(aif_focus_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_focus_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_focus_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
