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
    std::string bin_dir;
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

struct FileCollect
{
    // copy src_files[] in src_basedir to dst_dir and keep its directory struct
    //  e.g. {.src_base="inc1", .src_files=["x/file1.h"] .dst="out"} will
    //  copy "<cgn_script_dir>/inc1/x/file1.h" to "<target>/out/x/file1.h"
    //
    struct PatternSrc {
        // the 'src_files' based on, for example
        //   "repo/include", "/" (abs-path allowed)
        std::string src_basedir;
        
        // relevant path on <src_basedir>
        // '*' accepted, for example
        //   "google/protobuf/*.h"
        //   "**.h"
        std::vector<std::string> src_files;
    };
    struct Pattern : PatternSrc {
        std::string dst_dir = "";
    };

    struct Context {
        using T = Pattern;

        const std::string name;
        const cgn::Configuration cfg;

        std::vector<Pattern> mapper;
        void add(
            const std::string src_dir, 
            const std::vector<std::string> &src_files,
            const std::string &dst_dir = ""
        );

        cgn::TargetInfos add_target_dep(const std::string &label, const cgn::Configuration &cfg);

        Context(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
        : name(opt.factory_name), cfg(cfg), opt(opt) {}

    private:
        const cgn::CGNTargetOpt opt;  //self opt
        std::vector<std::string> _order_only_dep;
    };
    using context_type = Context;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);

}; //FileCollect


struct BinDevelCollect
{
    using FilePattern = FileCollect::PatternSrc; 

    struct Context
    {
        const std::string name;
        const cgn::Configuration cfg;
        
        std::vector<FilePattern> include, bin, lib;

        // std::vector<std::string> bin_files, lib_files, lib64_files;
        // void add_includes(
        //     const std::string &src_basedir,
        //     const std::vector<std::string> &src_files,
        //     const std::string dst_dir = "");

        // TargetInfos[CxxInfo]
        //   .include[] {*.h}         => this.include
        //   .shared[]  {*.so, *.lib} => this.lib64
        //   .static[]  {*.a, *.lib}  => this.lib64
        //   .runtime[] {*.dll, ELF, *.exe} => this.bin
        // TargetInfos[DefaultInfo]
        //   .output[] => this.bin / this.lib64
        void add_from_target(const std::string &label);
    
        Context(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
        : name(opt.factory_name), cfg(cfg), opt(opt), gp(cfg, opt) {}

    private: friend class BinDevelCollect;
        const cgn::CGNTargetOpt opt;  //self opt
        FileCollect::context_type gp;
    };
    using context_type = Context;

    GENERAL_CGN_BUNDLE_API static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
};

#define filegroup(name, x) CGN_RULE_DEFINE(FileCollect, name, x)
