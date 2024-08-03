#pragma once
#include "../../cgn.h"
#include "../../provider_dep.h"
#include "windef.h"

// Usually as parameter for external-build-system input,
//  like cmake -DZLIB_INCLUDE=... -DZSTD_LIBS=...
// It cannot be merged.
struct BinDevelInfo : cgn::BaseInfo {
    std::string base;
    std::string include_dir;
    std::string lib_dir;

    static const char *name() { return "BinDevelInfo"; }
    BinDevelInfo() : cgn::BaseInfo{_glb_bindevel_vtable()} {}

private:
    GENERAL_CGN_BUNDLE_API const static cgn::BaseInfo::VTable *_glb_bindevel_vtable();
};

struct BinDevelContext : cgn::TargetInfoDep<true>
{
    void set_include_dir(const std::string &dir);
    void add_to_include(
        const std::string &file, const std::string &rel_path);
    void set_lib_dir(const std::string &dir);
    void set_lib_dir_by_target(const std::string &label);
    void add_to_lib(const std::string &binfile);

    using cgn::TargetInfoDep<true>::TargetInfoDep;

private:
    BinDevelInfo info;
};
