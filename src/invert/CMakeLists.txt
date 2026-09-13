add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_invert_filters STATIC filters.cpp invert.cpp)
  add_library(AvsFilters::InvertFilters ALIAS aif_invert_filters)
  set_target_properties(aif_invert_filters PROPERTIES EXPORT_NAME InvertFilters)
  target_link_libraries(aif_invert_filters PUBLIC AvsFilters::InvertKernels)
  target_include_directories(aif_invert_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_invert_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_invert_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/invert/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/invert)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_invert_plugin MODULE plugin.cpp)
  target_include_directories(aif_invert_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_invert_plugin PRIVATE AvsFilters::InvertFilters)
  set_target_properties(aif_invert_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_invert")
  aif_target_defaults(aif_invert_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_invert_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_invert_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
