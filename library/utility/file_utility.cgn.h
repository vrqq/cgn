#pragma once
#ifdef _WIN32
    #ifdef CGN_UTILITY_IMPL
        #define CGN_UTILITY_API  __declspec(dllexport)
    #else
        #define CGN_UTILITY_API
    #endif
#else
    #define CGN_UTILITY_API __attribute__((visibility("default")))
#endif

#include <vector>
#include <string>
#include <unordered_map>
#include <cgn>
#include "cgn_path.h"

struct FileUtility
{
    const std::string &name;
    const cgn::Configuration &cfg;

    CGN_UTILITY_API void copy_on_build(
        const std::vector<std::string> &src, 
        const cgn::CGNPath &src_base, 
        const cgn::CGNPath &dst_dir
    );

    CGN_UTILITY_API void flat_copy_on_build(
        const std::vector<cgn::CGNPath> &src_list, 
        const cgn::CGNPath &dst_dir
    );

    // constexpr static int devel_from_bindevel    =     0b1;
    // constexpr static int devel_from_cxxinclude  =    0b10;
    // constexpr static int devel_from_linknrun    =  0b1100;
    // constexpr static int devel_from_output      = 0b10000;
    // cgn::CGNTarget collect_devel_on_build(
    //     const std::string &label, 
    //     int collect_flag,
    //     cgn::CGNPath perferred_basedir = cgn::make_path_base_out(),
    //     std::string perferred_lib_dirname = ""
    // );

    struct DevelOpt
    {
        bool allow_bindevel    = false;
        bool allow_cxxinclude  = false;
        bool allow_linknrun    = false;
        bool allow_output      = false;

        //TODO
        // std::string pkgconfig_template;
        bool as_return_value = true;
        cgn::CGNPath target_dir = cgn::make_path_base_out();
        std::string perferred_libdir;
    };
    static DevelOpt new_devel_opt() { return DevelOpt{}; }
    CGN_UTILITY_API cgn::CGNTarget collect_devel_on_build(
        const std::string &label, 
        DevelOpt opt
    );


    // --TODO--
    // cgn::CGNPath copy_rename_on_build(
    //     const cgn::CGNPath &src_file,
    //     const cgn::CGNPath &dst_file
    // );
    // cgn::CGNPath analysis_regex_file(
    //     const cgn::CGNPath &template_file,
    //     std::unordered_map<std::string, std::string> vars,
    //     const cgn::CGNPath &out_file
    // );
    // std::vector<cgn::CGNPath> analysis_gen_pkgconfig();
    // std::vector<cgn::CGNPath> analysis_gen_cmakeconfig();
    // --END TODO--

    // If path inside cgn-out/target_out, add to output of .phony target.
    // Otherwise call API.add_placeholder_file() and set CGNTarget.ninja_dep_level = ORDER_ONLY.
    // If all paths are inside cgn-out/target_out, CGNTarget.ninja_dep_level would set to NONE.
    std::vector<cgn::CGNPath> ninja_outputs;

    // CGNTarget.result.outputs[]
    std::vector<cgn::CGNPath> analysis_outputs;

    FileUtility(cgn::CGNTargetOptIn *opt) 
    : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

private: friend class FileUtilityInterpreter;
    cgn::CGNTargetOptIn *opt;

    // copy_xxx()
    struct CopyRecord {
        const char *command;
        std::string desc;
        std::function<std::vector<std::string>(cgn::CGNTargetOpt *opt)> arg_gen;
    };
    std::vector<CopyRecord> copy_records;

    // CGNTarget.infos<BinDevelInfo>
    bool         have_devel = false;
    cgn::CGNPath devel_basedir;
    std::string  devel_lib_dirname;

    // TBD: not required
    // CGNTarget.infos<CxxInfo>
    //      .include_dir = {devel_basedir / "include"};
    //      .ldflags, .cflags, ... = inherit from early target
    bool have_cxxinfo = false;
    cxx::CxxInfo devel_cxxinfo;
    
    // TBD: not required
    // CGNTarget.infos<LinkAndRunInfo>
    //      .shared_files = devel_shared[]
    //      .static_files = devel_static[]
    //      .runtime_files = devel_runtime[]
    // bool have_linknrun = false;
    // std::vector<cgn::CGNPath> devel_shared, devel_static, devel_runtime;
}; // struct FileUtility

struct FileUtilityInterpreter
{
    using context_type = FileUtility;

    constexpr static cgn::ConstLabelGroup<3> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle",
                "@cgn.d//library/cxx.cgn.bundle",
                "@cgn.d//library/utility/file_utility.cgn.cc"};
    }
    CGN_UTILITY_API static void interpret(context_type &x);
};

#define file_utility(name, x) CGN_RULE_DEFINE(::FileUtilityInterpreter, name, x)
