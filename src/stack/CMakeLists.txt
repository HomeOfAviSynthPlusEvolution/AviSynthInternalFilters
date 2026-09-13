if(AIF_BUILD_FILTERS)
  add_library(aif_stack_filters STATIC filters.cpp stack_vertical.cpp stack_horizontal.cpp show_five_versions.cpp)
  add_library(AvsFilters::StackFilters ALIAS aif_stack_filters)
  set_target_properties(aif_stack_filters PROPERTIES EXPORT_NAME StackFilters)
  target_link_libraries(aif_stack_filters PRIVATE AvsFilters::BlankClipKernels)
  target_include_directories(aif_stack_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_stack_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_stack_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/stack/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/stack)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_stack_plugin MODULE plugin.cpp)
  target_include_directories(aif_stack_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_stack_plugin PRIVATE AvsFilters::StackFilters)
  set_target_properties(aif_stack_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_stack")
  aif_target_defaults(aif_stack_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_stack_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_stack_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
