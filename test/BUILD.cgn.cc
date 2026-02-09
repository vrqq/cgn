#include <cgn>

cxx_executable("path_toolset", x) {
    x.srcs = {"test_cgn_path_toolset.cpp"};
    x.add_dep("../:cgn_static", cxx::private_dep);
    x.add_dep("@third_party//googletest:gtest", cxx::private_dep);
}


// TODO: build failed
// cxx_executable("cxx_test1", x) {
//     x.srcs = {"test_cxx.cpp"};
//     x.add_dep("../:cgn_static", cxx::private_dep);
//     x.add_dep("@third_party//googletest:gtest", cxx::private_dep);
// }

// for workspace dir 'home'
cxx_executable("test_p1", x) {
    if(x.cfg["os"] == "win")
        x.defines = {"GOTO_DIR=@cgn.d\\test\\home"};
    else
        x.defines = {"GOTO_DIR=@cgn.d/test/home"};
    x.srcs = {"test_p1.cpp"};
    x.add_dep("../:cgn_static", cxx::private_dep);
    x.add_dep("@third_party//googletest:gtest", cxx::private_dep);
}
