#include <cgn>

cxx_executable("cppdep", x) {
    x.srcs = {"main.cpp"};
    x.add_dep("@third_party//fmt", cxx::private_dep);
} 