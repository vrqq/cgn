#include "interpreter_a.cgn.h"
#include "../gtest_env.h"

TEST_INTERPRETER_A void InterpreterA::interpreter(context_type &x)
{
    if (gtest_env && gtest_env->on_interpreter_a)
        gtest_env->on_interpreter_a(x), gtest_env->on_interpreter_a = nullptr;
}
