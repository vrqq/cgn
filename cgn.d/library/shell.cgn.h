#pragma once
#include <string>
#include <vector>

#include "../cgn.h"
#include "../rule_marco.h"

namespace shell {

//Generate a ninja target which run command in shell
struct ShellBinary
{
    struct Context {
        const std::string name;

        std::string cwd, main, mnemonic;
        std::vector<std::string> args;
        std::unordered_map<std::string, std::string> env;
        std::unordered_set<std::string> inputs, outputs;  //Implicit dependencies
        cgn::Configuration cfg;

        Context(cgn::Configuration cfg, cgn::CGNTargetOpt opt) 
        : name(opt.factory_name), cfg(cfg) {}
    };

    using context_type = Context;

    // The absolute name of current interpreter
    constexpr static const char *script_label = "@cgn.d//library/shell.cgn.rsp";
    constexpr static const char *shell_rule = "@cgn.d//library/rule_shell.ninja";

    static std::string shell_escape(const std::string &in) { return in; }

    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
};

}

#define sh_binary(name, x) CGN_RULE_DEFINE(shell::ShellBinary, name, x)

#ifdef CGN_PCH_MODE
    CGN_SPECIALIZATION_PCH(shell::ShellBinary)
#endif
