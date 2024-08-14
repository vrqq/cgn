#include <cgn>

git("zlib.git", x) {
    x.repo = "https://github.com/madler/zlib.git";
    x.commit_id = "51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf";
}

cxx_static("z", x) {
    x.pub.include_dirs = {"repo"};
    x.srcs = {"repo/*.c"};
    if (x.cfg["os"] == "win")
        x.srcs += {"src/win32/zlib1.def"};
    if (x.cfg["os"] == "linux")
        x.cflags = {"-Wno-deprecated-non-prototype"};
    x.defines = {
            "_LARGEFILE64_SOURCE=1",
            "_CRT_SECURE_NO_DEPRECATE", 
            "_CRT_NONSTDC_NO_DEPRECATE"
    };
}
alias("zlib", x) {
    x.actual_label = ":z";
}

cxx_executable("zlib_test", x) {
    x.srcs = {"repo/test/example.c"};
    x.add_dep(":z", cxx::private_dep);
}

cxx_executable("zlib_test_infcover", x) {
    x.srcs = {"repo/test/infcover.c"};
    x.add_dep(":z", cxx::private_dep);
}

cxx_executable("zlib_test_minigzip", x) {
    x.srcs = {"repo/test/minigzip.c"};
    x.add_dep(":z", cxx::private_dep);
}

filegroup("devel", x) {
    x.add("repo", {"*.h"}, "include");

    auto info = x.add_target_dep(":zlib", x.cfg);
    auto *lib = info.get<cgn::LinkAndRunInfo>(false);
    x.flat_add_rootbase(lib->static_files, 
                        (x.cfg["cpu"]=="x86_64"?"lib64":"lib"));
}
// bin_devel("devel", x) {
//     x.add_from_target(":zlib");
// }