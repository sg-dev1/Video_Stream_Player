cmake_minimum_required(VERSION 3.0.2)
project(wv-adapter-lib)

set(USE_LIBSDL_RENDERER True CACHE BOOL ***REMOVED***Use the SDL2 library for rendering***REMOVED***)
set(DISABLE_CLOCK_SYNC False CACHE BOOL ***REMOVED***Disable sync of audio and video***REMOVED***)
set(DISABLE_VIDEO False CACHE BOOL ***REMOVED***Disable video rendering***REMOVED***)
set(DISABLE_AUDIO False CACHE BOOL ***REMOVED***Disable audio rendering***REMOVED***)
set(DISABLE_QUEUE_COUNTERS False CACHE BOOL ***REMOVED***Disable queue counters***REMOVED***)
set(LOG_LEVEL ***REMOVED***Debug***REMOVED*** CACHE String ***REMOVED***Choose log level. One of 'Debug', 'Info', 'Warning', 'Error'***REMOVED***)
set(BUILD_TYPE ***REMOVED***Release***REMOVED*** CACHE STRING ***REMOVED***Choose the build type. One of 'Debug', 'Release'***REMOVED***)
set(BUILD_TYPE_VALUES ***REMOVED***Debug;Release;RelWithDebInfo;MinSizeRel***REMOVED***)
set(BUILD_TEST_CASES True CACHE BOOL ***REMOVED***Build test cases for the wvAdapter library as well.***REMOVED***)
set_property(CACHE BUILD_TYPE PROPERTY STRINGS ${BUILD_TYPE_VALUES})

set(CMAKE_BUILD_TYPE ${BUILD_TYPE})
set(CMAKE_CXX_STANDARD 11)

message(STATUS ***REMOVED***Using BUILD_TYPE ${BUILD_TYPE}***REMOVED***)

if (LOG_LEVEL STREQUAL ***REMOVED***Debug***REMOVED***)
  message(STATUS ***REMOVED***Using Debug log level***REMOVED***)
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_DEBUG)
elseif (LOG_LEVEL STREQUAL ***REMOVED***Info***REMOVED***)
  message(STATUS ***REMOVED***Using Info log level***REMOVED***)
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_INFO)
elseif (LOG_LEVEL STREQUAL ***REMOVED***Warning***REMOVED***)
  message(STATUS ***REMOVED***Using Warning log level***REMOVED***)
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_WARNING)
elseif (LOG_LEVEL STREQUAL ***REMOVED***Error***REMOVED***)
  message(STATUS ***REMOVED***Using Error log level***REMOVED***)
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_ERROR)
else()
  message(STATUS ***REMOVED***Unknown LOG_LEVEL ${LOG_LEVEL} given. Using default Info.***REMOVED***)
  ADD_DEFINITIONS(-DLOG_LEVEL=LOG_LEVEL_INFO)
endif()

if (DISABLE_QUEUE_COUNTERS)
  message(***REMOVED***Queue counters will be disabled in this build***REMOVED***)
  ADD_DEFINITIONS(-DDISABLE_QUEUE_COUNTERS)
endif()

# check for preconditions (e.g. required programs)
find_program(PYTHON ***REMOVED***python***REMOVED***)
if (PYTHON MATCHES ***REMOVED***NOTFOUND$***REMOVED***)
  message( FATAL_ERROR ***REMOVED***Python must be installed to proceed!***REMOVED***)
endif()

message(STATUS ***REMOVED***Python path: ${PYTHON}***REMOVED***)
message(STATUS ***REMOVED***Source directory: ${CMAKE_CURRENT_SOURCE_DIR}***REMOVED***)
message(STATUS ***REMOVED***Bin directory: ${CMAKE_CURRENT_BINARY_DIR}***REMOVED***)

# set some directory paths
set(LIB_DIR ***REMOVED***${CMAKE_CURRENT_SOURCE_DIR}/../../lib***REMOVED***)
set(INCLUDE_DIR ***REMOVED***${CMAKE_CURRENT_SOURCE_DIR}/include***REMOVED***)
set(SRC_DIR ***REMOVED***${CMAKE_CURRENT_SOURCE_DIR}/src***REMOVED***)
set(SCRIPTS_DIR ***REMOVED***${CMAKE_CURRENT_SOURCE_DIR}/scripts***REMOVED***)

# fetch host arch (and if available pi version)
execute_process(
  COMMAND ${PYTHON} ${SCRIPTS_DIR}/detect_host_arch.py
  OUTPUT_VARIABLE HOST_ARCH
)
message(STATUS ***REMOVED***Detected host architecture: ${HOST_ARCH}***REMOVED***)

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

if (NOT HOST_ARCH STREQUAL ***REMOVED***x64***REMOVED*** AND NOT HOST_ARCH STREQUAL ***REMOVED***arm***REMOVED***)
  message(FATAL_ERROR ***REMOVED***Unknown platform ${HOST_ARCH} found.***REMOVED***)
endif()

message(STATUS ***REMOVED***----------------------------***REMOVED***)
message(STATUS ***REMOVED***Using definitions: ${DEFINES}***REMOVED***)
message(STATUS ***REMOVED***Using include directories: ${INCLUDE_DIRS}***REMOVED***)
message(STATUS ***REMOVED***Using libraries: ${LIBRARIES}***REMOVED***)
message(STATUS ***REMOVED***Using sources: ${SOURCES}***REMOVED***)
message(STATUS ***REMOVED***Using headers: ${HEADERS}***REMOVED***)
message(STATUS ***REMOVED***----------------------------***REMOVED***)

# create library
add_definitions(${DEFINES})
include_directories(${INCLUDE_DIRS})
add_Library(WvAdapter SHARED ${HEADERS} ${SOURCES})
target_link_libraries(WvAdapter ${LIBRARIES})

if (BUILD_TEST_CASES)
  include(${SRC_DIR}/cmake/GoogleTest.cmake)
endif()
