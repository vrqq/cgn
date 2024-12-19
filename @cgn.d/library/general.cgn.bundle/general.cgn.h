#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// exported zone
#include "git_fetch.cgn.h"
#include "bin_devel.cgn.h"
// end exported zone

#include "../../cgn.h"
#include "windef.h"

//Generate a ninja target which run command in shell
struct ShellBinary
{
    struct Context {
        const std::string &name;
        
        cgn::Configuration &cfg;

        //cwd: current working directory
        //mnemonic: 'DESCRIPTION' field in ninja file
        // cmd_analysis : cmdline running in analyse phase (mainexe at [0])
        //                auto shell escape
        // cmd_build    : cmdline written in build.ninja   (mainexe at [0])
        //                auto shell escape
        std::string cwd, mnemonic;
        std::unordered_map<std::string, std::string> env;
        std::vector<std::string> cmd_analysis, cmd_build;
        
        //${in} and ${out} in build.ninja of current target
        //path relative to current dir or absolute
        std::vector<std::string> inputs, outputs;

        Context(cgn::CGNTargetOptIn *opt) 
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

        cgn::CGNTarget add_dep(const std::string &label) { return add_dep(label); }
        cgn::CGNTarget add_dep(const std::string &label, const cgn::Configuration &cfg) {
            return opt->quick_dep(label, cfg);
        }

    private: friend class ShellBinary;
        cgn::CGNTargetOptIn *opt;
    };

    using context_type = Context;

    // The absolute name of current interpreter
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
}; //ShellBinary

struct CopyInterpreter
{
    struct Context {
        const std::string name;

        cgn::Configuration &cfg;

        // Copy file from 'from' to 'to' one by one,
        // the size of from and to must be equal.
        // path relative to current dir or absolute
        std::vector<std::string> from, to;

        Context(cgn::CGNTargetOptIn *opt) 
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

    private: friend class CopyInterpreter;
        cgn::CGNTargetOptIn *opt;
    };

    using context_type = Context;
    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
};


//Generate a ninja target which run command in shell
// struct CustomNinja {
//     struct context_type {
//         const std::string name;
//         const cgn::Configuration cfg;

//         //cwd: current working directory
//         //mnemonic: 'DESCRIPTION' field in ninja file
//         // cmd_analysis : cmdline running in analyse phase (mainexe at [0])
//         //                auto shell escape
//         // cmd_build    : cmdline written in build.ninja   (mainexe at [0])
//         //                user can use ninja variable like ${in}, ${out} here,
//         //                so respectly user should escape '$' manually
//         std::string cwd, mnemonic;
//         std::unordered_map<std::string, std::string> env;
//         std::vector<std::string> cmd_analysis, cmd_build;
        
//         //${in} and ${out} in build.ninja of current target
//         std::vector<std::string> inputs, outputs;
//         std::vector<std::string> implicit_inputs, implicit_outputs;

//         cgn::TargetInfos add_dep(const std::string label, cgn::Configuration cfg);

//         context_type(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt) 
//         : name(opt.factory_name), cfg(cfg) {}
//     };
//     constexpr static cgn::ConstLabelGroup<1> preload_labels() {
//         return {"@cgn.d//library/general.cgn.bundle"};
//     }
//     static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
// }; //CustomNinja


struct AliasInterpreter
{
    struct AliasContext {
        const std::string &name;
        std::string actual_label;
        cgn::Configuration &cfg;

        void load_named_config(const std::string &cfg_name);

        AliasContext(cgn::CGNTargetOptIn *opt)
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}
        
    private: friend class AliasInterpreter;
        cgn::CGNTargetOptIn *opt;
    };
    using context_type = AliasContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
}; //AliasInterpreter

struct DynamicAliasInterpreter
{
    struct DynamicAliasContext {
        const std::string &name;
        const cgn::Configuration &last_cfg;
        cgn::CGNTarget actual_target;

        cgn::CGNTarget load_target(const std::string &label) { 
            return load_target(label, last_cfg);
        }
        GENERAL_CGN_BUNDLE_API cgn::CGNTarget 
        load_target(const std::string &label, const cgn::Configuration &cfg);

        DynamicAliasContext(cgn::CGNTargetOptIn *opt)
        : name(opt->factory_name), last_cfg(opt->cfg) {}
        
        private: friend class DynamicAliasInterpreter;
        cgn::CGNTargetOptIn *opt;
    };
    using context_type = DynamicAliasContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
}; //DynamicAliasInterpreter


struct GroupInterpreter
{
    struct GroupContext {
        const std::string &name;
        const cgn::Configuration &cfg;

        std::vector<cgn::CGNTarget> add_deps(
            std::initializer_list<std::string> labels
        ) { return add_deps(labels, this->cfg); }

        GENERAL_CGN_BUNDLE_API std::vector<cgn::CGNTarget> add_deps(
            std::initializer_list<std::string> labels,
            const cgn::Configuration &cfg
        );

        GroupContext(cgn::CGNTargetOptIn *opt)
        : name(opt->factory_label), cfg(opt->cfg) {}

    private: friend class GroupInterpreter;
        cgn::CGNTargetOptIn *opt;
        std::vector<std::string> deps_ninja_entry;
    };
    using context_type = GroupContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }
    
    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
}; //GroupInterpreter


//
//
struct LinkAndRuntimeFiles {
    struct context_type : cgn::LinkAndRunInfo
    {
        const std::string &name;
        const cgn::Configuration &cfg;

        context_type(cgn::CGNTargetOptIn *opt)
        : name(opt->factory_name), cfg(opt->cfg) {}
    };

    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
};

#define sh_binary(name, x)  CGN_RULE_DEFINE(ShellBinary, name, x)
#define copy_files(name, x) CGN_RULE_DEFINE(CopyInterpreter, name, x)
#define dynamic_alias(name, x) CGN_RULE_DEFINE(DynamicAliasInterpreter, name, x)
#define alias(name, x) CGN_RULE_DEFINE(AliasInterpreter, name, x)
#define group(name, x) CGN_RULE_DEFINE(GroupInterpreter, name, x)
// #define link_and_runtime_files(name, x) CGN_RULE_DEFINE(LinkAndRuntimeFiles, name, x)

// #ifdef CGN_PCH_MODE
//     CGN_SPECIALIZATION_PCH(shell::ShellBinary)
// #endif
