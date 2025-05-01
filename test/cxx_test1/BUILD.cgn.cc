#include "@cgn.d/library/cxx/cxx.cgn.h"

cxx_shared("func1", x) {
    x.srcs = {"func1.cpp"};
    x.defines = {"FUNC1_IMPL"};
    x.pub.defines = {"HAVE_FUNC1"};
}

cxx_executable("cxx_test1", x) {
    x.srcs = {"hello.cpp"};
    x.add_dep(":func1", cxx::private_dep);
}
