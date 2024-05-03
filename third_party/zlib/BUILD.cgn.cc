#include <cgn>

git("zlib.git", x) {
    x.repo = "https://github.com/madler/zlib.git";
    x.commit_id = "51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf";
}

cxx_static("zlib", x) {
    x.pub.include_dirs = {"src"};
    x.srcs = {"src/*.c"};
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

cxx_executable("zlib_test", x) {
    x.srcs = {"src/test/example.c"};
    x.add_dep(":zlib", cxx::private_dep);
}

cxx_executable("zlib_test_infcover", x) {
    x.srcs = {"src/test/infcover.c"};
    x.add_dep(":zlib", cxx::private_dep);
}

cxx_executable("zlib_test_minigzip", x) {
    x.srcs = {"src/test/minigzip.c"};
    x.add_dep(":zlib", cxx::private_dep);
}
