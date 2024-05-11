#include <cgn>

cxx_executable("absl_lnk", x) {
    x.srcs = {"main.cpp"};
    x.add_dep("//third_party/abseil-cpp", cxx::private_dep);
}