#pragma once
#include <functional>

#ifdef MAIN_IMPL
__declspec(dllexport)
#else
__declspec(dllimport)
#endif
void global_reg(std::function<void*()> fn_new);