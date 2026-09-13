add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_planes_filters STATIC filters.cpp swap_uv.cpp swap_uv_to_y.cpp swap_y_to_uv.cpp combine_planes.cpp)
  add_library(AvsFilters::PlanesFilters ALIAS aif_planes_filters)
  set_target_properties(aif_planes_filters PROPERTIES EXPORT_NAME PlanesFilters)
  target_link_libraries(aif_planes_filters PUBLIC AvsFilters::PlanesKernels)
  target_include_directories(aif_planes_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_planes_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_planes_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/planes/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/planes)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_planes_plugin MODULE plugin.cpp)
  target_include_directories(aif_planes_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_planes_plugin PRIVATE AvsFilters::PlanesFilters)
  set_target_properties(aif_planes_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_planes")
  aif_target_defaults(aif_planes_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_planes_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_planes_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
