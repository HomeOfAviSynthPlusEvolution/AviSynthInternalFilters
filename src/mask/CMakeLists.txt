add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_mask_filters STATIC filters.cpp mask.cpp color_key_mask.cpp reset_mask.cpp mask_hs.cpp)
  add_library(AvsFilters::MaskFilters ALIAS aif_mask_filters)
  set_target_properties(aif_mask_filters PROPERTIES EXPORT_NAME MaskFilters)
  target_link_libraries(aif_mask_filters PUBLIC AvsFilters::MaskKernels)
  target_include_directories(aif_mask_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_mask_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_mask_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/mask/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mask)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_mask_plugin MODULE plugin.cpp)
  target_include_directories(aif_mask_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_mask_plugin PRIVATE AvsFilters::MaskFilters)
  set_target_properties(aif_mask_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_mask")
  aif_target_defaults(aif_mask_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_mask_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_mask_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
