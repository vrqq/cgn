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

# define _STR(x) #x
# define STR(x) _STR(x)

#define CGN_RULE_TITLE1(x, z) x##_##z
#define CGN_RULE_TITLE(x, z) CGN_RULE_TITLE1(x, z)

// standard target factory with interpeter class
#define CGN_RULE_DEFINE(InterpreterD, NameD, CtxD, ...) \
void CGN_RULE_TITLE(_tf, __LINE__)(InterpreterD::context_type& CtxD); \
static std::shared_ptr<void> CGN_RULE_TITLE(_tfreg, __LINE__) \
    = api.bind_target_factory<InterpreterD>(NameD, &CGN_RULE_TITLE(_tf, __LINE__), ## __VA_ARGS__); \
void CGN_RULE_TITLE(_tf, __LINE__)(InterpreterD::context_type& CtxD)
