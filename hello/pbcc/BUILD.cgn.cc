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
