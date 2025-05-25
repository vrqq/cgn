// BinDevelInfo
//  NO-DEPS
//
#ifdef _WIN32
    #ifdef CGN_BINDEVEL_IMPL
        #define CGN_BINDEVEL_API  __declspec(dllexport)
    #else
        #define CGN_BINDEVEL_API
    #endif
#else
    #define CGN_BINDEVEL_API __attribute__((visibility("default")))
#endif

#pragma once
#include "../../cgn.h"
#include "../cxx/cxx.cgn.h"

// Usually as parameter for external-build-system input,
//  like cmake -DZLIB_INCLUDE=... -DZSTD_LIBS=...
// It cannot be merged.
struct BinDevelInfo : cgn::BaseInfo {
    std::string install_dir;

    // perferred dir, maybe empty for multilib or other cases
    // relavent path of WorkingRoot or absolute path, not a relavent path of install_dir
    std::string include_dir;
    std::string bin_dir;
    std::string lib_dir;
    std::string man_dir;

    bool within_cmakeconfig = false;
    bool within_pkgconfig   = false;

    static const char *name() { return "BinDevelInfo"; }
    BinDevelInfo() : cgn::BaseInfo{_glb_bindevel_vtable()} {}

private:
    CGN_BINDEVEL_API const static cgn::BaseInfo::VTable *_glb_bindevel_vtable();
};
