add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_channel_display_filters STATIC filters.cpp show_channel.cpp)
  add_library(AvsFilters::ChannelDisplayFilters ALIAS aif_channel_display_filters)
  set_target_properties(aif_channel_display_filters PROPERTIES EXPORT_NAME ChannelDisplayFilters)
  target_link_libraries(aif_channel_display_filters PUBLIC AvsFilters::ChannelDisplayKernels PRIVATE AvsFilters::BlankClipKernels)
  target_include_directories(aif_channel_display_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_channel_display_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_channel_display_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/channel_display/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/channel_display)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_channel_display_plugin MODULE plugin.cpp)
  target_include_directories(aif_channel_display_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_channel_display_plugin PRIVATE AvsFilters::ChannelDisplayFilters)
  set_target_properties(aif_channel_display_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_channel_display")
  aif_target_defaults(aif_channel_display_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_channel_display_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_channel_display_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
