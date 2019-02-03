#ifndef WV_ADAPTER_LIB_COMMON_H
#define WV_ADAPTER_LIB_COMMON_H

// Generic helper definitions for shared library support
// see http://gcc.gnu.org/wiki/Visibility
#if defined _WIN32 || defined __CYGWIN__
#define WV_ADAPTER_HELPER_DLL_IMPORT __declspec(dllimport)
    #define WV_ADAPTER_HELPER_DLL_EXPORT __declspec(dllexport)
    #define WV_ADAPTER_HELPER_DLL_LOCAL
#else
#if __GNUC__ >= 4
    #define WV_ADAPTER_HELPER_DLL_IMPORT __attribute__ ((visibility ("default")))
    #define WV_ADAPTER_HELPER_DLL_EXPORT __attribute__ ((visibility ("default")))
    #define WV_ADAPTER_HELPER_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
    #define WV_ADAPTER_HELPER_DLL_IMPORT
    #define WV_ADAPTER_HELPER_DLL_EXPORT
    #define WV_ADAPTER_HELPER_DLL_LOCAL
#endif
#endif

// Now we use the generic helper definitions above to define WV_ADAPTER_API and WV_ADAPTER_LOCAL.
// WV_ADAPTER_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for static build)
// WV_ADAPTER_LOCAL is used for non-api symbols.

#ifdef WV_ADAPTER_DLL // defined if WV_ADAPTER is compiled as a DLL
    #ifdef WV_ADAPTER_DLL_EXPORTS // defined if we are building the WV_ADAPTER DLL (instead of using it)
        #define WV_ADAPTER_API WV_ADAPTER_HELPER_DLL_EXPORT
    #else
        #define WV_ADAPTER_API WV_ADAPTER_HELPER_DLL_IMPORT
    #endif // WV_ADAPTER_DLL_EXPORTS
    #define WV_ADAPTER_LOCAL WV_ADAPTER_HELPER_DLL_LOCAL
    // exports only required for test cases
    #ifdef WV_ADAPTER_TESTS
        #define WV_ADAPTER_TEST_API WV_ADAPTER_API
    #else
        #define WV_ADAPTER_TEST_API
    #endif // WV_ADAPTER_TESTS
#else // WV_ADAPTER_DLL is not defined: this means WV_ADAPTER is a static lib.
    #define WV_ADAPTER_API
    #define WV_ADAPTER_LOCAL
    #define WV_ADAPTER_TEST_API
#endif // WV_ADAPTER_DLL


// TODO merge with AdapterInterface::STREAM_TYPE
enum class MEDIA_TYPE {
  kAudio,
  kVideo
};

#endif //WV_ADAPTER_LIB_COMMON_H
