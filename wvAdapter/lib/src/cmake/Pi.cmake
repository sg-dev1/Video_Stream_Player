# fetch if available pi version
execute_process(
        COMMAND ${PYTHON} ${SCRIPTS_DIR}/pi_version.py
        OUTPUT_VARIABLE PI_VERSION
)

if (NOT PI_VERSION STREQUAL "")
    message(STATUS "Building for Raspberry Pi version ${PI_VERSION}")

    list(APPEND SOURCES
            ${SRC_DIR}/renderer/omx/ilclient.c
            ${SRC_DIR}/renderer/omx/omx_audio_renderer.cc
            ${SRC_DIR}/renderer/mmal/mmal_renderer.cc
            ${SRC_DIR}/renderer/mmal/mmal_audio_renderer.cc
            ${SRC_DIR}/renderer/mmal/mmal_video_renderer.cc
            )
    list(APPEND DEFINES
            -DRASPBERRY_PI
            -DOMX_SKIP64BIT)
    list(APPEND INCLUDE_DIRS
            ${INCLUDE_DIR}/renderer/mmal
            ${INCLUDE_DIR}/renderer/omx
            # Dispmanx includes
            /opt/vc/include
            /opt/vc/include/interface/vmcs_host
            /opt/vc/include/interface/vcos/pthreads
            /opt/vc/include/interface/vmcs_host/linux
            # Openmax includes
            /opt/vc/include/IL
            )
    list(APPEND LIBRARIES
            # Dispmanx libs
            -lbcm_host -lvcos -lvchiq_arm
            # EGL libs
            -lEGL -lGLESv2
            # Openmax includes
            -lopenmaxil
            # MMAL includes
            -lmmal -lmmal_core -lmmal_components -lmmal_util
            -lmmal_vc_client
            # Misc incldues
            -lrt -lm
            # set directory
            -L/opt/vc/lib
            )
endif()