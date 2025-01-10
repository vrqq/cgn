#ifdef _WIN32
    #ifdef NMAKE_CGN_IMPL
        #define NMAKE_CGN_API  __declspec(dllexport)
    #else
        #define NMAKE_CGN_API
    #endif
#else
    #define NMAKE_CGN_API __attribute__((visibility("default")))
#endif

#include "../cgn.h"
#include "cxx.cgn.bundle/cxx.cgn.h"

// variables assigned by Interpeter
// https://learn.microsoft.com/en-us/cpp/build/reference/special-nmake-macros
//  MAKEDIR = $target_out/build
//  ${ctx.install_prefix_varname} = $target_out/install
//  AS  = cxx::test_param(cfg["cxx_toolchain"]) Macro Assembler
//  CC  = cxx::test_param(cfg["cxx_toolchain"])
//  CPP = cxx::test_param(cfg["cxx_toolchain"])
//  CXX = cxx::test_param(cfg["cxx_toolchain"]) 
//  RC  = cxx::test_param(cfg["cxx_toolchain"]) Resource Compiler
struct NMakeContext
{
    const std::string &name;

    cgn::Configuration &cfg;

    //the file path base on 'cwd'
    std::string makefile = "Makefile";

    //the path where to run nmake.exe
    std::string cwd;

    // input files
    std::vector<std::string> inputs;

    // nmake output files
    std::vector<std::string> outputs;

    // A variable inside Makefile to present 'INSTALL_PREFIX'
    std::string install_prefix_varname;

    // some vars defined to nmake.exe
    std::unordered_map<std::string, std::string> predef_vars;
    std::unordered_map<std::string, std::string> override_vars;

    // the nmake target which to install
    std::string install_target_name = "install";

    // the nmake target which to clear build
    std::string clean_target_name   = "clean";

    cgn::CGNTarget add_dep(const std::string &label, const cgn::Configuration &cfg, bool keep_order = true) {
        auto rv = opt->quick_dep(label, cfg);
        if (keep_order)
            this->opt->quickdep_ninja_full += {rv.ninja_entry};
        return rv;
    }

    NMAKE_CGN_API NMakeContext(cgn::CGNTargetOptIn *opt)
    : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

// private: friend class NMakeInterpreter;
    cgn::CGNTargetOptIn *opt;
};

struct NMakeInterpreter
{
    using context_type = NMakeContext;
    
    constexpr static cgn::ConstLabelGroup<2> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle",
                "@cgn.d//library/nmake.cgn.cc"};
    }
    NMAKE_CGN_API static void interpret(context_type &x);
};

#define nmake(name, x) CGN_RULE_DEFINE(::NMakeInterpreter, name, x)