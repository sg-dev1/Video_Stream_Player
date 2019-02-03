if (USE_LIBSDL_RENDERER AND PI_VERSION STREQUAL "")
    list(APPEND DEFINES
            -DLIBSDL_RENDERER
            -D_REENTRANT)
    list(APPEND INCLUDE_DIRS
            ${INCLUDE_DIR}/renderer/sdl2
            )
    list(APPEND SOURCES
            ${SRC_DIR}/renderer/sdl2/sdl_audio_renderer.cc
            ${SRC_DIR}/renderer/sdl2/sdl_video_renderer.cc
            )
    list(APPEND HEADERS
            ${INCLUDE_DIR}/renderer/sdl2/sdl_audio_renderer.h
            ${INCLUDE_DIR}/renderer/sdl2/sdl_video_renderer.h
            )
    list(APPEND LIBRARIES
            -lSDL2
            )
endif()