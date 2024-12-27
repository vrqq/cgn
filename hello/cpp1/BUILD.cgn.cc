// C/C++ Interpreter Example : special char in filename

#include <cgn>

// target link this one would build successful
cxx_shared("lib1", x) {
    x.defines = {"LIB1_EXPOSE"};
    x.srcs = {"lib1\".cpp"};
}

// since the lib1 havn't expose any symbol, 
// target link with this one would build failure.
cxx_shared("lib1_linkerror", x) {
    x.srcs = {"lib1\".cpp"};
}

cxx_executable("cpp1", x) {
    x.srcs = {"main.cc"};
    x.add_dep("././anyfolder/../:lib1", cxx::private_dep);
}

