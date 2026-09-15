

if(AIF_BUILD_FILTERS)
  add_library(aif_field_filters STATIC filters.cpp complement_parity.cpp assume_parity.cpp assume_field_based.cpp assume_frame_based.cpp separate_fields.cpp double_weave_fields.cpp double_weave_frames.cpp fieldwise.cpp field_operations.cpp kernel/copy_fields.cpp)
  add_library(AvsFilters::FieldFilters ALIAS aif_field_filters)
  set_target_properties(aif_field_filters PROPERTIES EXPORT_NAME FieldFilters)
  target_include_directories(aif_field_filters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  aif_target_defaults(aif_field_filters)
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_field_filters EXPORT AviSynthInternalFiltersTargets
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES include/field/filters.h
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/field)
  endif()

endif()

if(AIF_BUILD_PLUGINS)
  add_library(aif_field_plugin MODULE plugin.cpp)
  target_include_directories(aif_field_plugin PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(aif_field_plugin PRIVATE AvsFilters::FieldFilters)
  set_target_properties(aif_field_plugin PROPERTIES PREFIX "" OUTPUT_NAME "aif_field")
  aif_target_defaults(aif_field_plugin)
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD)$")
    target_link_options(aif_field_plugin PRIVATE -Wl,-Bsymbolic)
  endif()
  if(AIF_ENABLE_INSTALL)
    install(TARGETS aif_field_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/avisynth
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  endif()

endif()
