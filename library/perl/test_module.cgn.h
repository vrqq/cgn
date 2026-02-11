#pragma once
#ifdef _WIN32
    #ifdef CGN_LIBRARY_PERL_TESTMOD_IMPL
        #define CGN_LIBRARY_PERL_TESTMOD_API  __declspec(dllexport)
    #else
        #define CGN_LIBRARY_PERL_TESTMOD_API
    #endif
#else
    #define CGN_LIBRARY_PERL_TESTMOD_API __attribute__((visibility("default")))
#endif

#include <set>
#include "../../cgn.h"


namespace perl {

class ModuleTestInterpreter
{
public:
    struct context_type {
        const std::string &name;
        cgn::Configuration &cfg;
        std::set<std::string> module_names;

        context_type(cgn::CGNTargetOpt *opt) : name(opt->name), cfg(opt->cfg), opt(opt) {}
    private: friend class ModuleTestInterpreter;
        cgn::CGNTargetOpt *opt;
    };

    constexpr static cgn::ConstLabelGroup<1> preload_labels() { 
        return {"@cgn.d//library/perl/test_module.cgn.cc"};
    }

    CGN_LIBRARY_PERL_TESTMOD_API static void interpret(context_type &x);
};

} //namespace

#define perl_test_module_existed(name, x) CGN_RULE_DEFINE(perl::ModuleTestInterpreter, name, x)
