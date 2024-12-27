#pragma once
#include "../../cgn.h"
#include "../cxx.cgn.bundle/cxx.cgn.h"
#include "windef.h"

// Usually as parameter for external-build-system input,
//  like cmake -DZLIB_INCLUDE=... -DZSTD_LIBS=...
// It cannot be merged.
struct BinDevelInfo : cgn::BaseInfo {
    std::string base;
    std::string include_dir;
    std::string bin_dir;
    std::string lib_dir;

    bool within_cmake_config = false;

    static const char *name() { return "BinDevelInfo"; }
    BinDevelInfo() : cgn::BaseInfo{_glb_bindevel_vtable()} {}

private:
    GENERAL_CGN_BUNDLE_API const static cgn::BaseInfo::VTable *_glb_bindevel_vtable();
};
