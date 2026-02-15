// create a custom build script
// NO-DEPS
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

struct CustomCommand : cgn::QuickDepContext
{
    const std::string &name;
    cgn::Configuration &cfg;

    std::vector<std::string> ninja_entry_targets;

    CustomCommand(cgn::CGNTargetOpt *opt) 
    : cgn::QuickDepContext(opt), name(opt->name), cfg(opt->cfg) {}
};

struct CustomInterpreter
{
    using context_type = CustomCommand;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/custom_command.cgn.cc"};
    }
    CGN_UTILITY_API static void interpret(context_type &x);
};

#define custom_command(name, x, ...) CGN_RULE_DEFINE(::CustomInterpreter, name, x, ## __VA_ARGS__)
