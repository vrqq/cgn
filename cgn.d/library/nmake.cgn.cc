#define NMAKE_CGN_IMPL
#include <cassert>
#include "../std_operator.hpp"
#include "nmake.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

static std::vector<std::string> rebase_and_njesc(
    const std::string base, const std::vector<std::string> &ls
) {
    std::vector<std::string> rv;
    for (auto &it : ls)
        rv.push_back(cgn::NinjaFile::escape_path( api.locale_path(base + it) ));
    return rv;
}

NMakeContext::NMakeContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
: cgn::TargetInfoDep<true>(cfg, opt), name(opt.factory_name) {}

cgn::TargetInfos NMakeInterpreter::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    assert(api.get_host_info().os == "win");
    if (x.outputs.empty())
        throw std::runtime_error{opt.factory_ulabel + " OUTPUT required."};

    cxx::CxxToolchainInfo cxx = cxx::CxxInterpreter::test_param(x.cfg);
    x.override_vars["CC"]  = cxx.c_exe;
    x.override_vars["CPP"] = cxx.cxx_exe;
    x.override_vars["CXX"] = cxx.cxx_exe;

    x.override_vars["MAKEDIR"] = opt.out_prefix + "build";
    x.override_vars[x.install_prefix_varname] = opt.out_prefix + "install";

    // import general rule.
    constexpr const char *rule = "@cgn.d//library/general.cgn.bundle/rule.ninja";
    static std::string rule_path = api.get_filepath(rule);
    opt.ninja->append_include(rule_path);

    // generate build args
    std::string args;
    for (auto it : x.override_vars)
        args += two_escape(it.first) + "=" + two_escape(it.second) + " ";

    // rebase output files
    std::vector<std::string> rv_output;
    std::vector<std::string> njesc_outputs;
    for (auto &it : x.outputs) {
        std::string path1 = api.locale_path(opt.out_prefix + "install/" + it);
        rv_output     += {path1};
        njesc_outputs += {opt.ninja->escape_path(path1)};
    }

    // generate build.ninja
    //  var["exe"] ${in} var["args"]
    auto *build = opt.ninja->append_build();
    build->rule = "run";
    build->variables["exe"] = "nmake.exe /NOLOGO /S /E /f";
    build->inputs = {opt.ninja->escape_path(
                        api.locale_path(opt.src_prefix + x.makefile)
                    )};
    build->variables["args"] = args + " install";
    build->outputs           = njesc_outputs;
    build->implicit_inputs   = rebase_and_njesc(opt.src_prefix, x.inputs);

    // phony .ENTRY
    auto *phony = opt.ninja->append_build();
    phony->rule = "PHONY";
    phony->inputs  = build->outputs;
    phony->outputs = {opt.out_prefix + opt.BUILD_ENTRY};

    cgn::TargetInfos &rv = x.merged_info;
    rv.get<cgn::DefaultInfo>(true)->outputs = rv_output;
    return rv;
}
