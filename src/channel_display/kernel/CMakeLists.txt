add_library(aif_channel_display_kernels STATIC dispatch.cpp highway.cpp)
add_library(AvsFilters::ChannelDisplayKernels ALIAS aif_channel_display_kernels)
set_target_properties(aif_channel_display_kernels PROPERTIES EXPORT_NAME ChannelDisplayKernels)
target_include_directories(aif_channel_display_kernels PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
aif_target_defaults(aif_channel_display_kernels)

if(AIF_SCALAR_ONLY)
 target_compile_definitions(aif_channel_display_kernels PRIVATE AIF_SCALAR_ONLY)
else()
 if(TARGET hwy::hwy)
  target_link_libraries(aif_channel_display_kernels PRIVATE hwy::hwy)
 else()
  target_link_libraries(aif_channel_display_kernels PRIVATE hwy)
 endif()
 target_include_directories(aif_channel_display_kernels PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
if(AIF_ENABLE_INSTALL)
  install(TARGETS aif_channel_display_kernels EXPORT AviSynthInternalFiltersTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES ../include/channel_display/kernel.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/channel_display)
endif()

target_link_libraries(aif_channel_display_kernels PRIVATE AviSynth::ConvertVideo)

target_link_libraries(aif_channel_display_kernels PRIVATE AvsFilters::BlankClipKernels)
