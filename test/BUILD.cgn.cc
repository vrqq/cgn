#include <cgn>

cxx_executable("path_toolset", x) {
    x.srcs = {"test_cgn_path_toolset.cpp"};
    x.add_dep("../:cgn_static", cxx::private_dep);
    x.add_dep("@third_party//googletest:gtest", cxx::private_dep);
}


// TODO: build failed
cxx_executable("cxx_test1", x) {
    x.srcs = {"test_cxx.cpp"};
    x.add_dep("../:cgn_static", cxx::private_dep);
    x.add_dep("@third_party//googletest:gtest", cxx::private_dep);
}
