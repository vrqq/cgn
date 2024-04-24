#include <cgn>

cxx_static("p1_static", x) {
    x.srcs = {"pkg1/subimpl.cpp"};
}

cxx_shared("p1_shared", x) {
    x.defines = {"PKG1_DLLEXPORT"};
    x.srcs = {"pkg1/subimpl.cpp"};
}

// select one from 2 above
cxx_shared("pkg1", x) {
    x.defines = {"PKG1_DLLEXPORT"};
    x.srcs = {"pkg1/subif.cpp"};
    x.add_dep(":p1_shared", cxx::inherit);
    // x.add_dep(":p1_static", cxx::inherit);
}

// entry
cxx_executable("cpp2", x) {
    x.srcs = {"cpp2.cpp"};
    x.add_dep(":pkg1", cxx::private_dep);
}