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

#include <cgn>
#include "cgn_path.h"

struct CustomCommand
{
    const std::string &name;
    cgn::Configuration &cfg;
    cgn::CGNTargetOptIn *opt;

    std::vector<cgn::CGNPath> cmd_inputs, cmd_outputs;
    std::function<void(CustomCommand &x, cgn::CGNTargetOpt *opt)> phase2_fn;

    cgn::CGNTarget add_dep(const std::string &label, const cgn::Configuration &cfg) {
        return opt->quick_dep(label, cfg);
    }
    cgn::CGNTarget add_dep(const std::string &label, const std::string &cfg_name) {
        return opt->quick_dep_namedcfg(label, cfg_name, true);
    }

    CGN_UTILITY_API void append_setenv(const std::string &key, const std::string &value);
    CGN_UTILITY_API void append_setenv(const std::unordered_map<std::string, std::string> &data);
    CGN_UTILITY_API void append_pushd(cgn::CGNPath &path);
    CGN_UTILITY_API void append_cmd(const std::vector<std::string> &args);

    CustomCommand(cgn::CGNTargetOptIn *opt)
    : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

private: friend struct CustomInterpreter;
    std::vector<std::pair<
        std::string, std::function<std::string(cgn::CGNTargetOpt *)>
    >> script_content;
};

struct CustomInterpreter
{
    using context_type = CustomCommand;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/custom_command.cgn.cc"};
    }
    CGN_UTILITY_API static void interpret(context_type &x);
};

#define custom_command(name, x) CGN_RULE_DEFINE(::CustomInterpreter, name, x)
