

if(AIF_BUILD_FILTERS)
  add_library(aif_frame_select_filters STATIC filters.cpp interleave.cpp select_every.cpp select_range_every.cpp)
  add_library(AvsFilters::FrameSelectFilters ALIAS aif_frame_select_filters)
  set_target_properties(aif_frame_select_filters PROPERTIES EXPORT_NAME FrameSelectFilters)
  target_include_directories(aif_frame_select_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  aif_target_defaults(aif_frame_select_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_frame_select_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/frame_select/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/frame_select)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_frame_select_plugin MODULE plugin.cpp)
  target_include_directories(aif_frame_select_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(aif_frame_select_plugin PRIVATE AvsFilters::FrameSelectFilters)
  set_target_properties(aif_frame_select_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_frame_select")
  aif_target_defaults(aif_frame_select_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_frame_select_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_frame_select_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
