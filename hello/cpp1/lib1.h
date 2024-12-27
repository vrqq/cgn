#ifdef LIB1_EXPOSE
    #ifdef _WIN32
        #define LIB1_IF __declspec(dllexport)
    #else
        #define LIB1_IF __attribute__((visibility("default")))
    #endif
#else
    #define LIB1_IF
#endif

#pragma once

LIB1_IF extern int func1();
