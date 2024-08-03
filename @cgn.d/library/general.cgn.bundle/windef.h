#ifdef _WIN32
    #ifdef GENERAL_CGN_BUNDLE_IMPL
        #define GENERAL_CGN_BUNDLE_API  __declspec(dllexport)
    #else
        #define GENERAL_CGN_BUNDLE_API
    #endif
#else
    #define GENERAL_CGN_BUNDLE_API __attribute__((visibility("default")))
#endif