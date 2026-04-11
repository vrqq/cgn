#include <cgn>

static std::string repo = "repo";

// LZ4 v1.10.0 - Multicores edition
git("lz4.git", x) {
    x.repo = "https://github.com/lz4/lz4";
    x.dest_dir = repo;
    x.commit_id = "ebb370ca83af193212df4dcbadcc5d87bc0de2f0";
}

cxx_static("lz4_static", x) {
    x.srcs = {
        repo + "/lib/lz4.c", 
        repo + "/lib/lz4file.c", 
        repo + "/lib/lz4frame.c", 
        repo + "/lib/lz4hc.c", 
        repo + "/lib/xxhash.c" 
    };
    x.pub.include_dirs = {repo + "/lib"};
}

cxx_executable("lz4_exe", x) {
    x.srcs = {
        repo + "/programs/bench.c",
        repo + "/programs/lorem.c",
        repo + "/programs/lz4cli.c",
        repo + "/programs/lz4io.c",
        repo + "/programs/threadpool.c",
        repo + "/programs/timefn.c",
        repo + "/programs/util.c"
    };
    x.add_dep(":lz4_static", cxx::private_dep);
}

alias("lz4", x) { x.actual_label = ":lz4_static"; }
