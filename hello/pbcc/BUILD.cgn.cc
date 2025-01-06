#include <cgn>

protobuf("pb1", x) {
    x.lang = x.Cxx;
    x.include_dirs = {"."};
    x.srcs = {"hello.proto"};
}

cxx_executable("pbcc", x) {
    x.srcs = {"main.cpp"};
    x.add_dep(":pb1", cxx::private_dep);
}

cxx_executable("abseil_test", x) {
    x.srcs = {"abseil_test.cpp"};
    x.add_dep("@third_party//abseil-cpp", cxx::private_dep);
}