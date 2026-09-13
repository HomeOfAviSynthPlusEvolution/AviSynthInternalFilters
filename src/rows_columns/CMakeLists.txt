add_subdirectory(kernel)

if(AIF_BUILD_FILTERS)
  add_library(aif_rows_columns_filters STATIC filters.cpp separate_columns.cpp weave_columns.cpp separate_rows.cpp weave_rows.cpp)
  add_library(AvsFilters::RowsColumnsFilters ALIAS aif_rows_columns_filters)
  set_target_properties(aif_rows_columns_filters PROPERTIES EXPORT_NAME RowsColumnsFilters)
  target_link_libraries(aif_rows_columns_filters PUBLIC AvsFilters::RowsColumnsKernels)
  target_include_directories(aif_rows_columns_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  aif_target_defaults(aif_rows_columns_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_rows_columns_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/rows_columns/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rows_columns)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_rows_columns_plugin MODULE plugin.cpp)
  target_include_directories(aif_rows_columns_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_rows_columns_plugin PRIVATE AvsFilters::RowsColumnsFilters)
  set_target_properties(aif_rows_columns_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_rows_columns")
  aif_target_defaults(aif_rows_columns_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_rows_columns_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_rows_columns_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
