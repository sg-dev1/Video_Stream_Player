# Tests are optional and use the system-provided GoogleTest package.
# Avoid downloading and building an unpinned dependency during CMake configure.
find_package(GTest REQUIRED)

if (BUILD_TEST_CASES)
  add_executable(blocking_stream_test tests/blocking_stream_test.cc)
  target_link_libraries(blocking_stream_test ${GTEST_BOTH_LIBRARIES} WvAdapter)

  add_executable(mp4_stream_test tests/mp4_stream_test.cc)
  target_link_libraries(mp4_stream_test ${GTEST_BOTH_LIBRARIES} WvAdapter)

  if (HOST_ARCH STREQUAL "arm" AND NOT PI_VERSION STREQUAL "")
    add_executable(mmal_renderer_test tests/mmal_renderer_test.cc)
    target_link_libraries(mmal_renderer_test ${GTEST_BOTH_LIBRARIES} WvAdapter)
  endif()

  add_executable(alsa_renderer_test tests/alsa_renderer_test.cc)
  target_link_libraries(alsa_renderer_test ${GTEST_BOTH_LIBRARIES} WvAdapter)
endif()