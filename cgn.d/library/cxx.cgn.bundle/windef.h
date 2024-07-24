#ifdef _WIN32
    #ifdef LANGCXX_CGN_BUNDLE_IMPL
        #define LANGCXX_CGN_BUNDLE_API  __declspec(dllexport)
    #else
        #define LANGCXX_CGN_BUNDLE_API
    #endif
#else
    #define LANGCXX_CGN_BUNDLE_API __attribute__((visibility("default")))
#endif