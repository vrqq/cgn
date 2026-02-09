#pragma once

#ifdef _WIN32
    #ifdef TEST_INTERPRETER_A
        #define TEST_INTERPRETER_A  __declspec(dllexport)
    #else
        #define TEST_INTERPRETER_A
    #endif
#else
    #define TEST_INTERPRETER_A __attribute__((visibility("default")))
#endif

#include "@cgn.d/cgn.h"

// define interpreter
struct InterpreterA {
    struct context_type {
        cgn::CGNTargetOpt *opt;
        context_type(cgn::CGNTargetOpt *opt) : opt(opt) {}
    };

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/general.cgn.cc"};
    }

    TEST_INTERPRETER_A static void interpreter(context_type &x);
};
