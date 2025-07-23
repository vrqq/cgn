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

struct RunExecInterperter
{
    struct context_type {
        const std::string &name;
        std::string actual_label;
        cgn::Configuration &cfg;
        
        std::vector<std::string>  cmd_build;
        std::vector<cgn::CGNPath> inputs, outputs;

        context_type(cgn::CGNTargetOptIn *opt)
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

    private: friend struct RunExecInterperter;
        cgn::CGNTargetOptIn *opt;
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

        AliasContext(cgn::CGNTargetOptIn *opt)
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}
        
    private: friend struct AliasInterpreter;
        std::string load_config_errormsg;
        cgn::CGNTargetOptIn *opt;
    };
    using context_type = AliasContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/general.cgn.cc"};
    }

    CGN_LIBRARY_GENERAL_API static void interpret(context_type &x);
}; //AliasInterpreter

struct GroupInterpreter
{
    struct GroupContext {
        const std::string &name;
        const cgn::Configuration &cfg;

        std::vector<cgn::CGNTarget> add_deps(
            std::initializer_list<std::string> labels
        ) { return add_deps(labels, this->cfg); }

        CGN_LIBRARY_GENERAL_API std::vector<cgn::CGNTarget> add_deps(
            std::initializer_list<std::string> labels,
            const cgn::Configuration &cfg
        );

        GroupContext(cgn::CGNTargetOptIn *_opt)
        : name(_opt->factory_label), cfg(_opt->cfg), opt(_opt) {}

    private: friend struct GroupInterpreter;
        cgn::CGNTargetOptIn *opt;
        std::vector<std::string> deps_ninja_entry;
    };
    using context_type = GroupContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/general.cgn.cc"};
    }
    
    CGN_LIBRARY_GENERAL_API static void interpret(context_type &x);
}; //GroupInterpreter


#define run_exec(name, x) CGN_RULE_DEFINE(RunExecInterperter, name, x)
#define alias(name, x)    CGN_RULE_DEFINE(AliasInterpreter, name, x)
#define group(name, x)    CGN_RULE_DEFINE(GroupInterpreter, name, x)

// #ifdef CGN_PCH_MODE
//     CGN_SPECIALIZATION_PCH(shell::ShellBinary)
// #endif
