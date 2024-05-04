#include "cgn"

git("tbb.git", x) {
    x.repo = "https://github.com/oneapi-src/oneTBB.git";
    x.commit_id = "9afd759b72c0c233cd5ea3c3c06b0894c9da9c54";
    x.dest_dir = "repo";
}

cxx_shared("tbb", x) {
    x.pub.include_dirs = x.include_dirs = {"repo/include"};
    x.srcs = {"repo/src/tbb/*.cpp"};

    if (x.cfg["toolchain"] == "gcc" || x.cfg["toolchain"] == "llvm") {
        if (x.cfg["cpu"] == "x86" || x.cfg["cpu"] == "x86_64")
            x.cflags += {"-mrtm", "-mwaitpkg"};
        if (x.cfg["optimization"] == "release")
            x.defines += {"_FORTIFY_SOURCE=2"};
    }

    if (x.cfg["toolchain"] == "gcc")
        x.cflags += {"-flifetime-dse", "-fstack-clash-protection"};
    if (x.cfg["toolchain"] == "llvm")
        x.cflags += {"-fexceptions"};

    if (x.cfg["os"] != "win")
        x.ldflags += {"-pthread"};
    if (x.cfg["os"] == "linux")
        x.ldflags += {"-ldl", "-lrt"};
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
              "repo/src/tbbmalloc/tbbmalloc.cpp"};
    x.defines = {"__TBBMALLOC_BUILD"};
}