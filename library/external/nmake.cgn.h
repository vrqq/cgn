// for external project build by MSVC nmake.exe
// DEPS ON : 
//   @cgn.d//library/cxx/cxx.cgn.cc
#ifdef _WIN32
    #ifdef NMAKE_CGN_IMPL
        #define NMAKE_CGN_API  __declspec(dllexport)
    #else
        #define NMAKE_CGN_API
    #endif
#else
    #define NMAKE_CGN_API __attribute__((visibility("default")))
#endif

#include "../../cgn.h"
#include "../cxx/cxx.cgn.h"

// variables assigned by Interpeter
// https://learn.microsoft.com/en-us/cpp/build/reference/special-nmake-macros
//  MAKEDIR = $target_out/build
//  ${ctx.install_prefix_varname} = $target_out/install
//  AS  = cxx::test_param(cfg["cxx_toolchain"]) Macro Assembler
//  CC  = cxx::test_param(cfg["cxx_toolchain"])
//  CPP = cxx::test_param(cfg["cxx_toolchain"])
//  CXX = cxx::test_param(cfg["cxx_toolchain"]) 
//  RC  = cxx::test_param(cfg["cxx_toolchain"]) Resource Compiler
struct NMakeContext : protected cgn::QuickDepContext
{
    const std::string &name;

    cgn::Configuration &cfg;

    // mode:
    //  * copy_and_build  : copy source to ${output}/src and compile it, 'src_dir' required.
    //  * build_out_of_src : by assigning build_dir to make build-file out of source code.
    
    // The source code dir, only used when build_dir_varname empty. (see below)
    cgn::CGNPath src_base;

    //the path where to run nmake.exe
    // cd ${src_base}/${nmake_run_dir}
    std::string nmake_run_dir = ".";

    // The 'makefile' file path base on nmake_run_dir
    // nmake -f ${makefile}
    std::string makefile;
    
    // the relavent file path base on 'cwd'
    // std::string makefile = "Makefile";

    // input files base on 'src_base'
    std::vector<std::string> inputs_rpath;
    std::vector<std::string> inputs_exclude_rpath;

    // nmake output files
    std::vector<std::string> outputs;

    // A variable inside Makefile to present 'INSTALL_PREFIX'
    // set [$install_prefix_varname] = {out_prefix}/install
    std::string install_prefix_varname;

    // A variable inside Makefile to present 'BUILD_DIR', keep empty if 
    // out-of-source compile is not supported.
    // <empty> : copy source code to {out_prefix}/src and run
    // <have_value> : set [$build_dir_varname] = {out_prefix}/build
    std::string build_dir_varname;

    // some vars defined to nmake.exe
    std::unordered_map<std::string, std::string> override_vars;

    // true  : set var["CC","CPP","CXX","AS"] from CxxInterpreter
    // false : keep it as original
    // bool auto_compiler_rel = true;

    // true  : set var["CFLAGS","CXXFLAGS","CPPFLAGS"] from CxxInterpreter
    // false : do not inherit any flags, keep them as original
    bool auto_cflags_rel = false;

    // the nmake target which to install
    std::string install_target_name = "install";

    // the nmake target which to clear build
    std::string clean_target_name   = "clean";

    std::vector<std::string> nmake_targets;

    cgn::CGNTarget add_dep(const std::string &label, const cgn::Configuration &cfg, bool keep_order = true) {
        auto rv = quick_dep(label, cfg);
        if (keep_order)
            ninja_fulldeps += {rv.ninja_entry};
        return rv;
    }

    void set_fail(const std::string &error_msg) {
        opt->set_fail(error_msg);
    }

    NMAKE_CGN_API NMakeContext(cgn::CGNTargetOpt *opt)
    : cgn::QuickDepContext(opt), name(opt->name), cfg(opt->cfg) {}

private: friend class NMakeInterpreter;
    std::vector<std::string> ninja_fulldeps;
};

struct NMakeInterpreter
{
    using context_type = NMakeContext;
    
    constexpr static cgn::ConstLabelGroup<3> preload_labels() {
        return {"@cgn.d//library/cxx/cxx.cgn.cc",
                "@cgn.d//library/utility/copy.cgn.cc",
                "@cgn.d//library/external/nmake.cgn.cc"};
    }
    NMAKE_CGN_API static void interpret(context_type &x);
};

#define nmake(name, x) CGN_RULE_DEFINE(::NMakeInterpreter, name, x)