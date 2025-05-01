
#pragma once

#ifdef FUNC1_IMPL
    #ifdef _WIN32
        #define FUNC1_API __declspec(dllexport)
    #else
        #define FUNC1_API __attribute__((visibility("default")))
    #endif
#else
    #define FUNC1_API
#endif

FUNC1_API int func1();