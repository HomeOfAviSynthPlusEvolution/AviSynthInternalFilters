

if(AIF_BUILD_FILTERS)
  add_library(aif_legacy_correction_filters STATIC filters.cpp fix_luminance.cpp fix_broken_chroma_upsampling.cpp peculiar_blend.cpp skew_rows.cpp kernel/correction.cpp)
  add_library(AvsFilters::LegacyCorrectionFilters ALIAS aif_legacy_correction_filters)
  set_target_properties(aif_legacy_correction_filters PROPERTIES EXPORT_NAME LegacyCorrectionFilters)
  target_include_directories(aif_legacy_correction_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  aif_target_defaults(aif_legacy_correction_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_legacy_correction_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/legacy_correction/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/legacy_correction)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_legacy_correction_plugin MODULE plugin.cpp)
  target_include_directories(aif_legacy_correction_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(aif_legacy_correction_plugin PRIVATE AvsFilters::LegacyCorrectionFilters)
  set_target_properties(aif_legacy_correction_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_legacy_correction")
  aif_target_defaults(aif_legacy_correction_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_legacy_correction_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_legacy_correction_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
