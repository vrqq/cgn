#ifdef _WIN32
    #ifdef CGN_LIBRARY_GENERAL_IMPL
        #define CGN_LIBRARY_GENERAL_API  __declspec(dllexport)
    #else
        #define CGN_LIBRARY_GENERAL_API
    #endif
#else
    #define CGN_LIBRARY_GENERAL_API __attribute__((visibility("default")))
#endif
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

#include "../../cgn.h"

struct RunExecInterpreter
{
    struct context_type {
        const std::string &name;
        cgn::Configuration &cfg;
        
        std::vector<std::string>  cmd_build;
        std::vector<cgn::CGNPath> inputs, outputs;

        context_type(cgn::CGNTargetOpt *opt)
        : name(opt->name), cfg(opt->cfg), opt(opt) {}

    private: friend struct RunExecInterpreter;
        cgn::CGNTargetOpt *opt;
    };

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/general.cgn.cc"};
    }

    CGN_LIBRARY_GENERAL_API static void interpret(context_type &x);
};

struct AliasInterpreter
{
    struct AliasContext {
        const std::string &name;
        std::string actual_label;
        cgn::Configuration &cfg;

        bool load_named_config(const std::string &cfg_name);

        AliasContext(cgn::CGNTargetOpt *opt)
        : name(opt->name), cfg(opt->cfg), opt(opt) {}
        
    private: friend struct AliasInterpreter;
        std::string load_config_errormsg;
        cgn::CGNTargetOpt *opt;
    };
    using context_type = AliasContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/general.cgn.cc"};
    }

    CGN_LIBRARY_GENERAL_API static void interpret(context_type &x);
}; //AliasInterpreter

struct GroupInterpreter
{
    struct GroupContext : protected cgn::QuickDepContext {
        const std::string &name;
        const cgn::Configuration &cfg;

        std::vector<cgn::CGNTarget> add_deps(
            std::vector<std::string> labels
        ) { return add_deps(labels, this->cfg); }

        std::vector<cgn::CGNTarget> add_deps(
            std::initializer_list<std::string> labels
        ) { return add_deps(labels, this->cfg); }

        CGN_LIBRARY_GENERAL_API std::vector<cgn::CGNTarget> add_deps(
            std::initializer_list<std::string> labels,
            const cgn::Configuration &cfg
        );
        CGN_LIBRARY_GENERAL_API std::vector<cgn::CGNTarget> add_deps(
            std::vector<std::string> labels,
            const cgn::Configuration &cfg
        );

        GroupContext(cgn::CGNTargetOpt *_opt)
        : cgn::QuickDepContext{_opt}, name(_opt->name), cfg(_opt->cfg) {}

    friend struct GroupInterpreter;
    };
    using context_type = GroupContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/general.cgn.cc"};
    }
    
    CGN_LIBRARY_GENERAL_API static void interpret(context_type &x);
}; //GroupInterpreter


#define run_exec(name, x) CGN_RULE_DEFINE(RunExecInterpreter, name, x)
#define alias(name, x)    CGN_RULE_DEFINE(AliasInterpreter, name, x)
#define group(name, x)    CGN_RULE_DEFINE(GroupInterpreter, name, x)

// #ifdef CGN_PCH_MODE
//     CGN_SPECIALIZATION_PCH(shell::ShellBinary)
// #endif
