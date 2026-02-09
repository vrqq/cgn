// define named factory
#include "@cgn.d/cgn.h"
#include "interpreter_a.cgn.h"
#include "../gtest_env.h"

// @p1//src:name1
auto binder = api.bind_target_factory<InterpreterA>("name1", [](InterpreterA::context_type &x) {
    if (gtest_env && gtest_env->on_factory_name1)
        gtest_env->on_factory_name1(x), gtest_env->on_factory_name1 = nullptr;
});

// @p1//src:name2
auto binder = api.bind_target_factory<InterpreterA>("name2", [](InterpreterA::context_type &x) {
if (gtest_env && gtest_env->on_factory_name2)
        gtest_env->on_factory_name2(x), gtest_env->on_factory_name1 = nullptr;
});
