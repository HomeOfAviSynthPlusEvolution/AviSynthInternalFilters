add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_blank_clip_filters STATIC filters.cpp blank_clip.cpp static_image.cpp single_frame.cpp)
  add_library(AvsFilters::BlankClipFilters ALIAS aif_blank_clip_filters)
  set_target_properties(aif_blank_clip_filters PROPERTIES EXPORT_NAME BlankClipFilters)
  target_link_libraries(aif_blank_clip_filters PUBLIC AvsFilters::BlankClipKernels)
  target_include_directories(aif_blank_clip_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_blank_clip_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_blank_clip_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/blank_clip/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/blank_clip)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_blank_clip_plugin MODULE plugin.cpp)
  target_include_directories(aif_blank_clip_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_blank_clip_plugin PRIVATE AvsFilters::BlankClipFilters)
  set_target_properties(aif_blank_clip_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_blank_clip")
  aif_target_defaults(aif_blank_clip_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_blank_clip_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_blank_clip_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
