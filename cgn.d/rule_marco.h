// Inspector from GTEST
// 
// Usage:
//  #define cxx_shared(x) CGN_RULE_TITLE(cxx::Interpreter, x)
//
// Expand to:
//  std::shared_ptr<void> _tfreg<PREFIX>_<LINE> = cgn.auto_target_factory<>(_tf<PREFIX>_<LINE>);
//  void _tf<PREFIX>_<LINE>(cxx::InterPreter::context_type &x)
//
// Note:
// CXXDefine from cgn-compiler
// CGN_RULE_PREFIX
// CGN_FILE_LABEL
#pragma once
#if (!defined(CGN_VAR_PREFIX) || !defined(CGN_ULABEL_PREFIX)) && !defined(CGN_PCH_MODE)
    // "CGN_VAR_PREFIX and CGN_ULABEL_PREFIX" should be defined by CGN compiler
    // here to make code intelligence
    #warning "INTERNAL ERROR"
    #define CGN_VAR_PREFIX ExampleProject
    #define CGN_ULABEL_PREFIX "//ExampleProject:"
#endif

// #ifdef __GNUC__
//     # define _STR(x) #x
//     # define STR(x) _STR(x)
// #else
//     # define STR(x) x
// #endif

#define CGN_RULE_TITLE1(x, y, p, q) x##y##p##q
#define CGN_RULE_TITLE(x, y, p, q) CGN_RULE_TITLE1(x, y, p, q)

#define CGN_RULE_DEFINE(InterpreterD, NameD, CtxD) \
void CGN_RULE_TITLE(_tf, CGN_VAR_PREFIX, _, __LINE__)(InterpreterD::context_type& CtxD); \
std::shared_ptr<void> CGN_RULE_TITLE(_tfreg, CGN_VAR_PREFIX, _, __LINE__) \
    = api.bind_target_factory<InterpreterD>(CGN_ULABEL_PREFIX, NameD, &CGN_RULE_TITLE(_tf, CGN_VAR_PREFIX, _, __LINE__)); \
void CGN_RULE_TITLE(_tf, CGN_VAR_PREFIX, _, __LINE__)(InterpreterD::context_type& CtxD)

// Marcos to speed up BUILD.cgn.cc
//================================

// Solution 2: template specialization by linking
#define CGN_SPECIALIZATION_IMPL(InterpreterD) \
template std::shared_ptr<void> CGN::bind_target_factory<InterpreterD>( \
    const std::string &, const std::string &,            \
    void(*factory)(InterpreterD::context_type&)                          \
);

// Solution 1: standalone PCH file
// Solution 2: template specialization by linking
#define CGN_SPECIALIZATION_PCH(InterpreterD) \
template<> std::shared_ptr<void> CGN::bind_target_factory<InterpreterD>( \
    const std::string &, const std::string &,            \
    void(*factory)(InterpreterD::context_type&)                          \
);