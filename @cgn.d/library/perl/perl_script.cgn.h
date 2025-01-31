#pragma once
#ifdef _WIN32
    #ifdef CGN_PERL_IMPL
        #define CGN_PERL_API  __declspec(dllexport)
    #else
        #define CGN_PERL_API
    #endif
#else
    #define CGN_PERL_API __attribute__((visibility("default")))
#endif

#include <cgn>

// The interim solution

struct PerlScriptContext
{
    const std::string &name;
    cgn::Configuration &cfg;
    std::vector<std::string> args;

    PerlScriptContext(cgn::CGNTargetOptIn *opt) 
    : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

private: friend struct PerlScriptInterpreter;
    cgn::CGNTargetOptIn *opt;

};

struct PerlScriptInterpreter
{
    using context_type = PerlScriptContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/perl/perl_script.cgn.cc"};
    }
    CGN_UTILITY_API static void interpret(context_type &x);
};