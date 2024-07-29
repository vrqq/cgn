#include <cgn>

// since the lib1 havn't expose any symbol, target cpp1 would build failure.
cxx_shared("lib1", x) {
    x.srcs = {"lib1\".cpp"};
}

cxx_executable("cpp1", x) {
    x.srcs = {"main.cc"};    
    x.add_dep("././anyfolder/../:lib1", cxx::private_dep);
}