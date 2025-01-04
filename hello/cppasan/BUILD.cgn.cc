#include <cgn>

cxx_executable("cppasan", x) {
    x.srcs = {"check_asan.cpp"};
}