add_library(aif_rows_columns_kernels STATIC dispatch.cpp scalar.cpp highway.cpp)
add_library(AvsFilters::RowsColumnsKernels ALIAS aif_rows_columns_kernels)
set_target_properties(aif_rows_columns_kernels PROPERTIES EXPORT_NAME RowsColumnsKernels)
target_include_directories(aif_rows_columns_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_rows_columns_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_rows_columns_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_rows_columns_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_rows_columns_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_rows_columns_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_rows_columns_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/rows_columns/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rows_columns)
endif()
