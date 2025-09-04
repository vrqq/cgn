// Package tools:
//   copy all LinkAndRunInfo.runtimes[] to current output folder
//
// c++ 17 and above in current version
// DEPS ON :
//   @cgn.d//library/utility:copy (@cgn.d//advcopy inside)
//
#pragma once
#ifdef _WIN32
    #ifdef CGN_LIBRARY_MAKEPKG_IMPL
        #define CGN_LIBRARY_MAKEPKG_API  __declspec(dllexport)
    #else
        #define CGN_LIBRARY_MAKEPKG_API
    #endif
#else
    #define CGN_LIBRARY_MAKEPKG_API __attribute__((visibility("default")))
#endif
#include "../../cgn.h"

// x.cfg["pkg_mode"] = "D + <dirname>"
class PackageInterpreter {
public:
    struct context_type {
        const std::string &name;
        cgn::Configuration &cfg;

        cgn::CGNTarget add_dep(const std::string &label) { return add_dep(label, this->cfg); }
        CGN_LIBRARY_MAKEPKG_API cgn::CGNTarget add_dep(const std::string &label, cgn::Configuration cfg);

        void add_deps(const std::vector<std::string> &labels) { return add_deps(labels, this->cfg); }
        CGN_LIBRARY_MAKEPKG_API void add_deps(const std::vector<std::string> &labels, cgn::Configuration cfg);
    
        context_type(cgn::CGNTargetOptIn *opt) 
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) { 
            cfg["pkg_mode"] = "D";
        }
    private: friend class PackageInterpreter;
        cgn::CGNTargetOptIn *opt;
        std::unordered_map<std::string, decltype(cgn::LinkAndRunInfo::runtime_files)> copy_record;
    };

    constexpr static cgn::ConstLabelGroup<2> preload_labels() { 
        return {"@cgn.d//library/utility/copy.cgn.cc",
                "@cgn.d//library/utility/package.cgn.cc"};
    }

    CGN_LIBRARY_MAKEPKG_API static void interpret(context_type &x);
};

#define package(name, x) CGN_RULE_DEFINE(PackageInterpreter, name, x)
