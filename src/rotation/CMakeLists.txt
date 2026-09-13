add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_rotation_filters STATIC filters.cpp turn.cpp flip_horizontal.cpp flip_vertical.cpp)
  add_library(AvsFilters::RotationFilters ALIAS aif_rotation_filters)
  set_target_properties(aif_rotation_filters PROPERTIES EXPORT_NAME RotationFilters)
  target_link_libraries(aif_rotation_filters PUBLIC AvsFilters::RotationKernels)
  target_include_directories(aif_rotation_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_rotation_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_rotation_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/rotation/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rotation)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_rotation_plugin MODULE plugin.cpp)
  target_include_directories(aif_rotation_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_rotation_plugin PRIVATE AvsFilters::RotationFilters)
  set_target_properties(aif_rotation_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_rotation")
  aif_target_defaults(aif_rotation_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_rotation_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_rotation_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
