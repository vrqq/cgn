#include "cgn"

// oneTBB 2022.0.0 (Nov 1, 2024)
git("tbb.git", x) {
    x.repo = "https://github.com/uxlfoundation/oneTBB.git";
    x.commit_id = "0c0ff192a2304e114bc9e6557582dfba101360ff";
    x.dest_dir = "repo";
}

static std::string _tbb_def_file(const cgn::Configuration cfg) {
    if (cfg["cxx_toolchain"] == "llvm" && cfg["os"] == "linux")
        return "expose_for_lld.def";
    
    auto _rv = [](std::string mid){ return "repo/src/tbb/def/" + mid + "-tbb.def"; };
    // repo/cmake/compiler/Clang.cmake
    // script for for llvm linker (lld) is not same as GNU ld
    if (cfg["cxx_toolchain"] == "llvm" && cfg["os"] == "mac")
        return _rv("mac64");

    if (cfg["cxx_toolchain"] == "msvc") {
        if (cfg["os"] == "win" && cfg["cpu"] == "x86")
            return _rv("win32");
        if (cfg["os"] == "win" && cfg["cpu"] == "x86_64")
            return _rv("win64");
    }

    if (cfg["os"] == "linux" && cfg["cpu"] == "x86")
        return _rv("lin32");
    if (cfg["os"] == "linux" && cfg["cpu"] == "x86_64")
        return _rv("lin64");

    return "";
}

cxx_shared("tbb", x) {
    x.pub.include_dirs = x.include_dirs = {"repo/include"};
    x.srcs = {"repo/src/tbb/*.cpp", _tbb_def_file(x.cfg)};

    if (x.cfg["cxx_toolchain"] == "gcc" || x.cfg["cxx_toolchain"] == "llvm") {
        if (x.cfg["cpu"] == "x86" || x.cfg["cpu"] == "x86_64")
            x.cflags += {"-w", "-mwaitpkg"};
        // if (x.cfg["optimization"] == "release")
        //     x.defines += {"_FORTIFY_SOURCE=2"};
    }

    if (x.cfg["cxx_toolchain"] == "gcc")
        x.cflags += {"-flifetime-dse", "-fstack-clash-protection"};
    if (x.cfg["cxx_toolchain"] == "llvm")
        x.cflags += {"-fexceptions"};

    if (x.cfg["os"] != "win")
        x.ldflags += {"-pthread"};
    if (x.cfg["os"] == "linux") {
        x.cflags += {"-fvisibility=default"};
        x.ldflags += {"-ldl", "-lrt"};
    }
    if (x.cfg["os"] == "mac")
        x.pub.defines += {"_XOPEN_SOURCE"};
    if (x.cfg["cpu"] == "x86_64"){
        x.pub.defines += {"__TBB_NO_IMPLICIT_LINKAGE"};
        x.defines += {"__TBB_USE_ITT_NOTIFY"};
    }
    x.pub.defines += {"USE_PTHREAD"};
    x.defines += {"__TBB_BUILD"};
}

cxx_shared("tbbmalloc", x) {
    x.pub.include_dirs =  x.include_dirs = {"repo/include"};
    x.srcs = {"repo/src/tbbmalloc/backend.cpp",
              "repo/src/tbbmalloc/backref.cpp",
              "repo/src/tbbmalloc/frontend.cpp",
              "repo/src/tbbmalloc/large_objects.cpp",
              "repo/src/tbbmalloc/tbbmalloc.cpp", 
              _tbb_def_file(x.cfg)};
    x.defines = {"__TBBMALLOC_BUILD"};
}
