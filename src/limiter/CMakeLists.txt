add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_limiter_filters STATIC filters.cpp limiter.cpp)
  add_library(AvsFilters::LimiterFilters ALIAS aif_limiter_filters)
  set_target_properties(aif_limiter_filters PROPERTIES EXPORT_NAME LimiterFilters)
  target_link_libraries(aif_limiter_filters PUBLIC AvsFilters::LimiterKernels)
  target_include_directories(aif_limiter_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_limiter_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_limiter_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/limiter/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/limiter)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_limiter_plugin MODULE plugin.cpp)
  target_include_directories(aif_limiter_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_limiter_plugin PRIVATE AvsFilters::LimiterFilters)
  set_target_properties(aif_limiter_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_limiter")
  aif_target_defaults(aif_limiter_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_limiter_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_limiter_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
