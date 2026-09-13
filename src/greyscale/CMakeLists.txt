add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_greyscale_filters STATIC filters.cpp greyscale.cpp)
  add_library(AvsFilters::GreyscaleFilters ALIAS aif_greyscale_filters)
  set_target_properties(aif_greyscale_filters PROPERTIES EXPORT_NAME GreyscaleFilters)
  target_link_libraries(aif_greyscale_filters PUBLIC AvsFilters::GreyscaleKernels)
  target_include_directories(aif_greyscale_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_greyscale_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_greyscale_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/greyscale/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/greyscale)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_greyscale_plugin MODULE plugin.cpp)
  target_include_directories(aif_greyscale_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_greyscale_plugin PRIVATE AvsFilters::GreyscaleFilters)
  set_target_properties(aif_greyscale_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_greyscale")
  aif_target_defaults(aif_greyscale_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_greyscale_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_greyscale_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
