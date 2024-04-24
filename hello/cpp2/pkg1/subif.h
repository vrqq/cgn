
#ifdef PKG1_DLLEXPORT
    #ifdef _WIN32
        #define PKG1_IF __declspec(dllexport)
    #else
        #define PKG1_IF __attribute__((visibility("default")))
    #endif
#else
    #define PKG1_IF
#endif

extern void fnX();
extern void fnY();