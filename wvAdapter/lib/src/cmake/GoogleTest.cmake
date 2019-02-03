#
# testing
# see https://github.com/google/googletest/blob/master/googletest/README.md#incorporating-into-an-existing-cmake-project
#

# Download and unpack googletest at configure time
configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
        ${CMAKE_BINARY_DIR}/googletest-build
        EXCLUDE_FROM_ALL)

#
# Create test binaries
#
#add_executable(dummy tests/dummy_test.cc)
#target_link_libraries(dummy gtest_main)
#add_test(NAME dummy_test COMMAND dummy)
add_executable(blocking_stream_test tests/blocking_stream_test.cc)
target_link_libraries(blocking_stream_test gtest_main WvAdapter)
add_executable(mp4_stream_test tests/mp4_stream_test.cc)
target_link_libraries(mp4_stream_test gtest_main WvAdapter)

if (HOST_ARCH STREQUAL "arm" AND NOT PI_VERSION STREQUAL "")
    add_executable(mmal_renderer_test tests/mmal_renderer_test.cc)
    target_link_libraries(mmal_renderer_test gtest_main WvAdapter)
endif()

add_executable(alsa_renderer_test tests/alsa_renderer_test.cc)
target_link_libraries(alsa_renderer_test gtest_main WvAdapter)