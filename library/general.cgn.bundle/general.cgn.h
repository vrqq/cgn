#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// exported zone
#include "git_fetch.cgn.h"
#include "bin_devel.cgn.h"
#include "url_download.cgn.h"
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

    private: friend struct ShellBinary;
        cgn::CGNTargetOptIn *opt;
    };

    using context_type = Context;

    // The absolute name of current interpreter
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
}; //ShellBinary

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
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
}; //AliasInterpreter

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

    private: friend struct GroupInterpreter;
        cgn::CGNTargetOptIn *opt;
        std::vector<std::string> deps_ninja_entry;
    };
    using context_type = GroupContext;
    
    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }
    
    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
}; //GroupInterpreter


#define sh_binary(name, x)  CGN_RULE_DEFINE(ShellBinary, name, x)
// #define copy_files(name, x) CGN_RULE_DEFINE(CopyInterpreter, name, x)
// #define dynamic_alias(name, x) CGN_RULE_DEFINE(DynamicAliasInterpreter, name, x)
#define alias(name, x) CGN_RULE_DEFINE(AliasInterpreter, name, x)
#define group(name, x) CGN_RULE_DEFINE(GroupInterpreter, name, x)
// #define link_and_runtime_files(name, x) CGN_RULE_DEFINE(LinkAndRuntimeFiles, name, x)

// #ifdef CGN_PCH_MODE
//     CGN_SPECIALIZATION_PCH(shell::ShellBinary)
// #endif
