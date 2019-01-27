list(APPEND INCLUDE_DIRS
        ${LIB_DIR}/Bento4/Source/C++/Core
        ${LIB_DIR}/Bento4/Source/C++/Adapters
        ${LIB_DIR}/Bento4/Source/C++/CApi
        ${LIB_DIR}/Bento4/Source/C++/Codecs
        ${LIB_DIR}/Bento4/Source/C++/Crypto
        ${LIB_DIR}/Bento4/Source/C++/MetaData
        ${LIB_DIR}/Bento4/Source/C++/System)

list(APPEND LIBRARIES
        -lap4_shared
        -L${LIB_DIR}/Bento4/cmakebuild)