

if(AIF_BUILD_FILTERS)
  add_library(aif_frame_rate_filters STATIC filters.cpp assume_scaled_fps.cpp assume_fps.cpp change_fps.cpp convert_fps.cpp rational.cpp)
  add_library(AvsFilters::FrameRateFilters ALIAS aif_frame_rate_filters)
  set_target_properties(aif_frame_rate_filters PROPERTIES EXPORT_NAME FrameRateFilters)
  target_link_libraries(aif_frame_rate_filters PUBLIC AviSynth::Composite)
  target_include_directories(aif_frame_rate_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_frame_rate_filters)
  if(AIF_SCALAR_ONLY)
    target_compile_definitions(aif_frame_rate_filters PRIVATE AIF_SCALAR_ONLY)
  elseif(TARGET hwy::hwy)
    target_link_libraries(aif_frame_rate_filters PRIVATE hwy::hwy)
  else()
    target_link_libraries(aif_frame_rate_filters PRIVATE hwy)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_frame_rate_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/frame_rate/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/frame_rate)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_frame_rate_plugin MODULE plugin.cpp)
  target_include_directories(aif_frame_rate_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_frame_rate_plugin PRIVATE AvsFilters::FrameRateFilters)
  set_target_properties(aif_frame_rate_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_frame_rate")
  aif_target_defaults(aif_frame_rate_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_frame_rate_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_frame_rate_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
