// C/C++ Interpreter Example
//
#include <cgn>

cxx_static("p1_static", x) {
    x.defines = {"PKG1_DLLEXPORT"};
    x.srcs = {"pkg1/sub_fn_y.cpp"};
}

cxx_shared("p1_shared", x) {
    x.defines = {"PKG1_DLLEXPORT"};
    x.srcs = {"pkg1/sub_fn_y.cpp"};
}

cxx_shared("pkg1", x) {
    x.defines = {"PKG1_DLLEXPORT"};
    x.srcs = {"pkg1/sub_fn_x.cpp"};

    // user can select one from two target aboves freely.
    // --------------------------------------------------
    // x.add_dep(":p1_shared", cxx::inherit);
    x.add_dep(":p1_static", cxx::inherit);
}

// entry
cxx_executable("cpp2", x) {
    x.srcs = {"cpp2.cpp"};
    x.add_dep(":pkg1", cxx::private_dep);
}