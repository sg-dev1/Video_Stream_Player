cmake_minimum_required(VERSION 3.0.2)
project(wv-adapter-lib)

set(USE_LIBSDL_RENDERER True CACHE BOOL "Use the SDL2 library for rendering")
set(DISABLE_CLOCK_SYNC False CACHE BOOL "Disable sync of audio and video")
set(DISABLE_VIDEO False CACHE BOOL "Disable video rendering")
set(DISABLE_AUDIO False CACHE BOOL "Disable audio rendering")
set(DISABLE_QUEUE_COUNTERS False CACHE BOOL "Disable queue counters")
set(LOG_LEVEL "Debug" CACHE String "Choose log level. One of 'Debug', 'Info', 'Warning', 'Error'")
set(BUILD_TYPE "Release" CACHE STRING "Choose the build type. One of 'Debug', 'Release'")
set(BUILD_TYPE_VALUES "Debug;Release;RelWithDebInfo;MinSizeRel")
set(BUILD_TEST_CASES True CACHE BOOL "Build test cases for the wvAdapter library as well.")
set_property(CACHE BUILD_TYPE PROPERTY STRINGS ${BUILD_TYPE_VALUES})

set(CMAKE_BUILD_TYPE ${BUILD_TYPE})
set(CMAKE_CXX_STANDARD 11)

message(STATUS "Using BUILD_TYPE ${BUILD_TYPE}")

if (LOG_LEVEL STREQUAL "Debug")
  message(STATUS "Using Debug log level")
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_DEBUG)
elseif (LOG_LEVEL STREQUAL "Info")
  message(STATUS "Using Info log level")
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_INFO)
elseif (LOG_LEVEL STREQUAL "Warning")
  message(STATUS "Using Warning log level")
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_WARNING)
elseif (LOG_LEVEL STREQUAL "Error")
  message(STATUS "Using Error log level")
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_ERROR)
else()
  message(STATUS "Unknown LOG_LEVEL ${LOG_LEVEL} given. Using default Info.")
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_INFO)
endif()

if (DISABLE_QUEUE_COUNTERS)
  message("Queue counters will be disabled in this build")
  ADD_DEFINITIONS(-DDISABLE_QUEUE_COUNTERS)
endif()

# check for preconditions (e.g. required programs)
find_program(PYTHON "python")
if (PYTHON MATCHES "NOTFOUND$")
  message( FATAL_ERROR "Python must be installed to proceed!")
endif()

message(STATUS "Python path: ${PYTHON}")
message(STATUS "Source directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Bin directory: ${CMAKE_CURRENT_BINARY_DIR}")

# set some directory paths
set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../lib")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scripts")

# fetch host arch (and if available pi version)
execute_process(
  COMMAND ${PYTHON} ${SCRIPTS_DIR}/detect_host_arch.py
  OUTPUT_VARIABLE HOST_ARCH
)
message(STATUS "Detected host architecture: ${HOST_ARCH}")

# include additional library files/ dependencies on specific platform
include(${SRC_DIR}/cmake/Bento4.cmake)
include(${SRC_DIR}/cmake/Pi.cmake)
include(${SRC_DIR}/cmake/SDL2.cmake)

# set up defaults
# build as dynamic library and export symbols
list(APPEND DEFINES
  -DWV_ADAPTER_DLL -DWV_ADAPTER_DLL_EXPORTS
  -fvisibility=hidden -fvisibility-inlines-hidden)
list(APPEND HEADERS
  ${INCLUDE_DIR}/common/assertion.h
  ${INCLUDE_DIR}/common/blocking_queue.h
  ${INCLUDE_DIR}/common/constants.h
  ${INCLUDE_DIR}/common/logging.h
  ${INCLUDE_DIR}/common/thread_utils.h
  ${INCLUDE_DIR}/common/util.h
  ${INCLUDE_DIR}/common/common.h
  ${INCLUDE_DIR}/common/queue_counter.h
  ${INCLUDE_DIR}/common/time_logger.h

  ${INCLUDE_DIR}/core/adapterInterface.h
  ${INCLUDE_DIR}/core/audio.h
  ${INCLUDE_DIR}/core/clock.h
  ${INCLUDE_DIR}/core/networking.h
  ${INCLUDE_DIR}/core/video.h
  ${INCLUDE_DIR}/core/widevineAdapter.h
  ${INCLUDE_DIR}/core/widevineDecryptor.h
  ${INCLUDE_DIR}/core/components.h

  ${INCLUDE_DIR}/demux/blocking_stream.h
  ${INCLUDE_DIR}/demux/mp4_stream.h

  ${INCLUDE_DIR}/renderer/alsa/alsa_audio_renderer.h
  ${INCLUDE_DIR}/renderer/audio_renderer.h
  ${INCLUDE_DIR}/renderer/default_renderer.h
  ${INCLUDE_DIR}/renderer/renderer.h
  ${INCLUDE_DIR}/renderer/video_renderer.h

  ${INCLUDE_DIR}/third-party/content_decryption_module.h
)
list(APPEND SOURCES
  ${SRC_DIR}/common/thread_utils.cc
  ${SRC_DIR}/common/util.cc
  ${SRC_DIR}/common/logging.cc
  ${SRC_DIR}/common/queue_counter.cc
  ${SRC_DIR}/common/time_logger.cc

  ${SRC_DIR}/core/adapterInterface.cc
  ${SRC_DIR}/core/audio.cc
  ${SRC_DIR}/core/clock.cc
  ${SRC_DIR}/core/networking.cc
  ${SRC_DIR}/core/video.cc
  ${SRC_DIR}/core/widevineAdapter.cc
  ${SRC_DIR}/core/widevineDecryptor.cc
  ${SRC_DIR}/core/components.cc

  ${SRC_DIR}/demux/blocking_stream.cc
  ${SRC_DIR}/demux/mp4_stream.cc

  ${SRC_DIR}/renderer/audio_renderer.cc
  ${SRC_DIR}/renderer/default_renderer.cc
  ${SRC_DIR}/renderer/video_renderer.cc
  ${SRC_DIR}/renderer/alsa/alsa_audio_renderer.cc
)
list(APPEND INCLUDE_DIRS
  ${INCLUDE_DIR}/common
  ${INCLUDE_DIR}/core
  ${INCLUDE_DIR}/demux
  ${INCLUDE_DIR}/renderer
  ${INCLUDE_DIR}/third-party
  ${INCLUDE_DIR}/renderer/alsa
)
list(APPEND LIBRARIES
  -ldl
  -lpthread
  -lcurl
  -lavutil
  -lavformat
  -lavcodec
  -lasound
)

if (DISABLE_CLOCK_SYNC)
  list(APPEND DEFINES -DDISABLE_CLOCK_SYNC)
endif()

if (DISABLE_AUDIO)
  list(APPEND DEFINES -DDISABLE_AUDIO)
endif()

if (DISABLE_VIDEO)
  list(APPEND DEFINES -DDISABLE_VIDEO)
endif()

if (BUILD_TEST_CASES)
  list(APPEND DEFINES -DWV_ADAPTER_TESTS)
endif()

if (NOT HOST_ARCH STREQUAL "x64" AND NOT HOST_ARCH STREQUAL "arm")
  message(FATAL_ERROR "Unknown platform ${HOST_ARCH} found.")
endif()

message(STATUS "----------------------------")
message(STATUS "Using definitions: ${DEFINES}")
message(STATUS "Using include directories: ${INCLUDE_DIRS}")
message(STATUS "Using libraries: ${LIBRARIES}")
message(STATUS "Using sources: ${SOURCES}")
message(STATUS "Using headers: ${HEADERS}")
message(STATUS "----------------------------")

# create library
add_definitions(${DEFINES})
include_directories(${INCLUDE_DIRS})
add_Library(WvAdapter SHARED ${HEADERS} ${SOURCES})
target_link_libraries(WvAdapter ${LIBRARIES})

if (BUILD_TEST_CASES)
  include(${SRC_DIR}/cmake/GoogleTest.cmake)
endif()
