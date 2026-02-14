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
    cgn::CGNPath project_dir;

    // 
    // The arguments for generate nmake_build.bat
    // cd ${project_dir}\${nmake_run_dir}
    // nmake.exe /F ${makefile} ${target_install};
    // if (return code != 0)
    //   nmake.exe /F ${makefile} ${override_vars} ${target_clean};
    //   nmake.exe /F ${makefile} ${override_vars} ${target_install};
    // --------------------------------------------------------------

    //the path where to run nmake.exe
    // cd ${src_base}/${nmake_run_dir}
    // (where src_base would influenced by $need_copy_src)
    std::string nmake_run_dir = ".";

    // The 'makefile' file path base on $nmake_run_dir
    // cd $nmake_run_dir; nmake /F $makefile
    std::string makefile;

    // some vars defined to nmake.exe
    std::unordered_map<std::string, std::string> override_vars;

    // A variable inside Makefile to present 'INSTALL_PREFIX'
    // interpreter(): override_vars[$install_prefix_varname] = {out_prefix}/install
    std::string install_prefix_varname;

    // A variable inside Makefile to present 'BUILD_DIR', keep empty if 
    // out-of-source compile is not supported.
    // interpreter(): override_vars[$build_dir_varname] = {out_prefix}/build
    std::string build_dir_varname;

    // the nmake target which to install one by one
    std::vector<std::string> target_installs;

    // the nmake target which to clear build
    std::string target_clean = "clean";

    // nmake output files (relative path base on $install_prefix)
    std::vector<std::string> outputs;

    // Using xcopy.exe to copy source code to ${output}/src before build.
    bool need_copy_src = false;
    std::vector<std::string> copy_exclude;

    // Extra ninja target watch file.
    // Auto added by add_dep(keep_order == true), but user can also add by hand for non-CGNTarget dependency.
    // $x.makefile would add by interpreter automatically.
    std::vector<cgn::CGNPath> extra_watch_files;

    // true  : set var["CC","CPP","CXX","AS"] from CxxInterpreter
    // false : keep it as original
    // bool autovar_compiler = true;

    // true  : set var["CFLAGS","CXXFLAGS","CPPFLAGS"] from CxxInterpreter
    // false : do not inherit any flags, keep them as original
    bool autovar_cflags = false;

    cgn::CGNTarget add_dep(const std::string &label, const cgn::Configuration &cfg, bool keep_order = true) {
        auto rv = quick_dep(label, cfg);
        if (keep_order)
            extra_watch_files += {rv.ninja_entry};
        return rv;
    }

    void set_fail(const std::string &error_msg) {
        opt->set_fail(error_msg);
    }

    NMAKE_CGN_API NMakeContext(cgn::CGNTargetOpt *opt)
    : cgn::QuickDepContext(opt), name(opt->name), cfg(opt->cfg) {}

    friend class NMakeInterpreter;
};

struct NMakeInterpreter
{
    using context_type = NMakeContext;
    
    constexpr static cgn::ConstLabelGroup<2> preload_labels() {
        return {"@cgn.d//library/cxx/cxx.cgn.cc",
                "@cgn.d//library/external/nmake.cgn.cc"};
    }
    NMAKE_CGN_API static void interpret(context_type &x);
};

#define nmake(name, x) CGN_RULE_DEFINE(::NMakeInterpreter, name, x)