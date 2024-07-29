
#ifdef _WIN32
    #ifdef PKG1_DLLEXPORT
        #define PKG1_IF __declspec(dllexport)
    #else
        #define PKG1_IF
    #endif
#else
    #define PKG1_IF __attribute__((visibility("default")))
#endif

PKG1_IF extern void fnX();
PKG1_IF extern void fnY();
