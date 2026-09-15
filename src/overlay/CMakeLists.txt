

if(AIF_BUILD_FILTERS)
  add_library(aif_overlay_filters STATIC filters.cpp overlay.cpp kernel/chroma_conversion.cpp kernel/chroma_scalar.cpp kernel/chroma_highway.cpp kernel/composite_overlay.cpp)
  add_library(AvsFilters::OverlayFilters ALIAS aif_overlay_filters)
  set_target_properties(aif_overlay_filters PROPERTIES EXPORT_NAME OverlayFilters)
  target_link_libraries(aif_overlay_filters PUBLIC AviSynth::Composite)
  target_include_directories(aif_overlay_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/kernel")
  aif_target_defaults(aif_overlay_filters)
  if(AIF_SCALAR_ONLY)
    target_compile_definitions(aif_overlay_filters PRIVATE AIF_SCALAR_ONLY)
  elseif(TARGET hwy::hwy)
    target_link_libraries(aif_overlay_filters PRIVATE hwy::hwy)
  else()
    target_link_libraries(aif_overlay_filters PRIVATE hwy)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_overlay_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/overlay/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/overlay)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_overlay_plugin MODULE plugin.cpp)
  target_include_directories(aif_overlay_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/kernel")
  target_link_libraries(aif_overlay_plugin PRIVATE AvsFilters::OverlayFilters)
  set_target_properties(aif_overlay_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_overlay")
  aif_target_defaults(aif_overlay_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_overlay_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_overlay_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
