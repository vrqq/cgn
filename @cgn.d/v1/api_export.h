// cgn.d DLLEXPORT
//         cgn_api.h : class CGN
//      ninja_file.h : class NinjaFile
//          logger.h : class Logger
//   configuration.h : class Configuration
//       base_type.h : ...
//
#ifdef CGN_EXE_IMPLEMENT
    #ifdef _WIN32
        #define CGN_EXPORT __declspec(dllexport)
    #else
        #define CGN_EXPORT __attribute__((visibility("default")))
    #endif
#else
    #ifdef _WIN32
        #define CGN_EXPORT __declspec(dllimport)
    #else
        #define CGN_EXPORT
    #endif
#endif
