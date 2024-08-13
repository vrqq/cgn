#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// exported zone
#include "git_fetch.cgn.h"
#include "bin_devel.cgn.h"
// end exported zone

#include "../../cgn.h"
#include "../../rule_marco.h"
#include "../../provider_dep.h"

#include "windef.h"

//Generate a ninja target which run command in shell
struct ShellBinary
{
    struct Context : cgn::TargetInfoDep<true> {
        const std::string name;

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
        std::vector<std::string> inputs, outputs;

        Context(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt) 
        : cgn::TargetInfoDep<true>(cfg, opt), name(opt.factory_name) {}
    };

    using context_type = Context;

    // The absolute name of current interpreter
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
}; //ShellBinary

struct CopyInterpreter
{
    struct Context : cgn::TargetInfoDep<true>{
        const std::string name;

        // Copy file from 'from' to 'to' one by one,
        // the size of from and to must be equal.
        std::vector<std::string> from, to;

        Context(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt) 
        : cgn::TargetInfoDep<true>(cfg, opt), name(opt.factory_name) {}

        friend class CopyInterpreter;
    };

    using context_type = Context;
    GENERAL_CGN_BUNDLE_API static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
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
        const std::string name;
        std::string actual_label;
        cgn::Configuration cfg;

        AliasContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
        : name(opt.factory_name), cfg(cfg) {}
    };
    using context_type = AliasContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
}; //AliasInterpreter

struct DynamicAliasInterpreter
{
    struct DynamicAliasContext {
        const std::string name;
        const cgn::Configuration last_cfg;
        cgn::TargetInfos actual_target_infos;

        cgn::CGNTarget load_target(const std::string &label) { 
            return load_target(label, last_cfg);
        }
        GENERAL_CGN_BUNDLE_API cgn::CGNTarget 
        load_target(const std::string &label, const cgn::Configuration &cfg);

        DynamicAliasContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
        : name(opt.factory_name), last_cfg(cfg) {}
        
        private: friend class DynamicAliasInterpreter;
        cgn::DefaultInfo self_def;
    };
    using context_type = DynamicAliasContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
}; //DynamicAliasInterpreter


struct GroupInterpreter
{
    struct GroupContext : cgn::TargetInfoDep<true> {
        const std::string name;

        std::vector<cgn::TargetInfos> add_deps(
            std::initializer_list<std::string> labels
        ) { return add_deps(labels, this->cfg); }

        GENERAL_CGN_BUNDLE_API std::vector<cgn::TargetInfos> add_deps(
            std::initializer_list<std::string> labels,
            const cgn::Configuration &cfg
        );

        GroupContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
        : cgn::TargetInfoDep<true>(cfg, opt), name(opt.factory_ulabel) {}

        friend class GroupInterpreter;
    };
    using context_type = GroupContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }
    
    GENERAL_CGN_BUNDLE_API static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
}; //GroupInterpreter


//
//
struct LinkAndRuntimeFiles {
    struct context_type : cgn::LinkAndRunInfo
    {
        const std::string name;
        const cgn::Configuration cfg;

        context_type(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
        : name(opt.factory_name), cfg(cfg) {}
    };

    GENERAL_CGN_BUNDLE_API static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
};

#define sh_binary(name, x)  CGN_RULE_DEFINE(ShellBinary, name, x)
#define copy_files(name, x) CGN_RULE_DEFINE(CopyInterpreter, name, x)
#define dynamic_alias(name, x) CGN_RULE_DEFINE(DynamicAliasInterpreter, name, x)
#define alias(name, x) CGN_RULE_DEFINE(AliasInterpreter, name, x)
#define group(name, x) CGN_RULE_DEFINE(GroupInterpreter, name, x)
#define link_and_runtime_files(name, x) CGN_RULE_DEFINE(LinkAndRuntimeFiles, name, x)

// #ifdef CGN_PCH_MODE
//     CGN_SPECIALIZATION_PCH(shell::ShellBinary)
// #endif
