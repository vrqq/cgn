#pragma once
#include <memory>
#include <functional>
#include "src1/interpreter_a.cgn.h"

struct GTestEnv {
    std::function<void(InterpreterA::context_type &x)> on_factory_name1, on_factory_name2;
    std::function<void(InterpreterA::context_type &x)> on_interpreter_a;
};
extern thread_local std::unique_ptr<GTestEnv> gtest_env;