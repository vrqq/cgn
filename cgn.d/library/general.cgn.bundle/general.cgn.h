#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// exported zone
#include "git_fetch.cgn.h"
// end exported zone

#include "../../cgn.h"
#include "../../rule_marco.h"
#include "../../provider_dep.h"

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

    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
}; //ShellBinary


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

    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
}; //AliasInterpreter


struct GroupInterpreter
{
    struct GroupContext : cgn::TargetInfoDep<true> {
        const std::string name;

        std::vector<cgn::TargetInfos> add_deps(
            std::initializer_list<std::string> labels
        ) { return add_deps(labels, this->cfg); }

        std::vector<cgn::TargetInfos> add_deps(
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
    
    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
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

    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
};

#define sh_binary(name, x) CGN_RULE_DEFINE(ShellBinary, name, x)
#define alias(name, x) CGN_RULE_DEFINE(AliasInterpreter, name, x)
#define group(name, x) CGN_RULE_DEFINE(GroupInterpreter, name, x)
#define link_and_runtime_files(name, x) CGN_RULE_DEFINE(LinkAndRuntimeFiles, name, x)

#ifdef CGN_PCH_MODE
    CGN_SPECIALIZATION_PCH(shell::ShellBinary)
#endif
