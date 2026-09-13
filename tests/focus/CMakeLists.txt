find_package(Threads REQUIRED)
add_executable(aif_focus_kernel_tests kernel_tests.cpp)
target_link_libraries(aif_focus_kernel_tests PRIVATE AvsFilters::FocusKernels Threads::Threads)
aif_target_defaults(aif_focus_kernel_tests)
add_test(NAME focus.kernels COMMAND aif_focus_kernel_tests)

add_executable(aif_focus_memory_tests memory_tests.cpp)
target_link_libraries(aif_focus_memory_tests PRIVATE AvsFilters::FocusKernels)
aif_target_defaults(aif_focus_memory_tests)
add_test(NAME focus.active_span COMMAND aif_focus_memory_tests)

add_executable(aif_focus_c_consumer c_consumer.c)
target_link_libraries(aif_focus_c_consumer PRIVATE AvsFilters::FocusKernels)
add_test(NAME focus.c_consumer COMMAND aif_focus_c_consumer)

if(AIF_BUILD_FILTERS)
  add_executable(aif_focus_cpu_policy_tests cpu_policy_tests.cpp)
  target_include_directories(aif_focus_cpu_policy_tests PRIVATE
    "${AIF_AVISYNTH_INCLUDE_DIR}" "${PROJECT_SOURCE_DIR}/src/focus")
  target_link_libraries(aif_focus_cpu_policy_tests PRIVATE AvsFilters::FocusKernels)
  aif_target_defaults(aif_focus_cpu_policy_tests)
  add_test(NAME focus.cpu_policy COMMAND aif_focus_cpu_policy_tests)
endif()

if(AIF_BUILD_PLUGINS AND AIF_TEST_RUNTIME)
  add_executable(aif_focus_filter_tests filter_tests.cpp)
  target_include_directories(aif_focus_filter_tests PRIVATE "${AIF_AVISYNTH_INCLUDE_DIR}")
  target_link_libraries(aif_focus_filter_tests PRIVATE ${CMAKE_DL_LIBS})
  aif_target_defaults(aif_focus_filter_tests)
  add_dependencies(aif_focus_filter_tests aif_focus_plugin)
  add_test(NAME focus.filters COMMAND aif_focus_filter_tests
    "${AIF_TEST_RUNTIME}" "$<TARGET_FILE:aif_focus_plugin>")
endif()
