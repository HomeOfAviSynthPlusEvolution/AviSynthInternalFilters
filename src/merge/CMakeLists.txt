add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_merge_filters STATIC filters.cpp merge_all.cpp merge_chroma.cpp merge_luma.cpp)
  add_library(AvsFilters::MergeFilters ALIAS aif_merge_filters)
  set_target_properties(aif_merge_filters PROPERTIES EXPORT_NAME MergeFilters)
  target_link_libraries(aif_merge_filters PUBLIC AvsFilters::MergeKernels)
  target_include_directories(aif_merge_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_merge_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_merge_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/merge/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/merge)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_merge_plugin MODULE plugin.cpp)
  target_include_directories(aif_merge_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_merge_plugin PRIVATE AvsFilters::MergeFilters)
  set_target_properties(aif_merge_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_merge")
  aif_target_defaults(aif_merge_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_merge_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_merge_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
