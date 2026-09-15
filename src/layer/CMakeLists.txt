

if(AIF_BUILD_FILTERS)
  add_library(aif_layer_filters STATIC filters.cpp layer.cpp subtract.cpp kernel/layer_frame.cpp)
  add_library(AvsFilters::LayerFilters ALIAS aif_layer_filters)
  set_target_properties(aif_layer_filters PROPERTIES EXPORT_NAME LayerFilters)
  target_link_libraries(aif_layer_filters PUBLIC AviSynth::Composite)
  target_include_directories(aif_layer_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_layer_filters)
  if(AIF_SCALAR_ONLY)
    target_compile_definitions(aif_layer_filters PRIVATE AIF_SCALAR_ONLY)
  elseif(TARGET hwy::hwy)
    target_link_libraries(aif_layer_filters PRIVATE hwy::hwy)
  else()
    target_link_libraries(aif_layer_filters PRIVATE hwy)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_layer_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/layer/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/layer)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_layer_plugin MODULE plugin.cpp)
  target_include_directories(aif_layer_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_layer_plugin PRIVATE AvsFilters::LayerFilters)
  set_target_properties(aif_layer_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_layer")
  aif_target_defaults(aif_layer_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_layer_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_layer_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
