#include "@cgn.d/library/cmake.cgn.h"

cmake("zlib", x) {
    x.sources_dir = "zlib";
    x.outputs = {"lib/libz.so", "lib/libz.a"};
    cgn::Configuration cfg;
    cgn::CGNTargetOpt opt;
    // CMakeContext aaa{cfg, opt};
    // aaa.pub.cflags = {"-Werror"};
    // cxx::CxxInfo cxxinfo;
    // cxxinfo.defines = {"NODEF"};
}
