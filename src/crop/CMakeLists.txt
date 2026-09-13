add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_crop_filters STATIC filters.cpp crop.cpp add_borders.cpp letterbox.cpp)
  add_library(AvsFilters::CropFilters ALIAS aif_crop_filters)
  set_target_properties(aif_crop_filters PROPERTIES EXPORT_NAME CropFilters)
  target_link_libraries(aif_crop_filters PUBLIC AvsFilters::CropKernels)
  target_include_directories(aif_crop_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_crop_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_crop_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/crop/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/crop)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_crop_plugin MODULE plugin.cpp)
  target_include_directories(aif_crop_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_crop_plugin PRIVATE AvsFilters::CropFilters)
  set_target_properties(aif_crop_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_crop")
  aif_target_defaults(aif_crop_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_crop_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_crop_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
