#include "../cgn.h"
#include "../rule_marco.h"
#include "../provider_dep.h"
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
struct NMakeContext : cgn::TargetInfoDep<true>
{
    const std::string &name;

    std::string makefile;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    std::string install_prefix_varname;
    std::unordered_map<std::string, std::string> predef_vars;
    std::unordered_map<std::string, std::string> override_vars;

    std::string install_target_name = "install";
    std::string clean_target_name   = "clean";

    NMakeContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt);
    friend class NMakeInterpreter;
};

struct NMakeInterpreter
{
    using context_type = NMakeContext;
    
    constexpr static cgn::ConstLabelGroup<2> preload_labels() {
        return {"@cgn.d//library/cxx.cgn.bundle",
                "@cgn.d//library/nmake.cgn.cc"};
    }
    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
};

#define nmake(name, x) CGN_RULE_DEFINE(::NMakeInterpreter, name, x)