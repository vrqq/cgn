#include <cgn>

static const std::string BZ2_ROOT = "repo";

// Bzip2 Jun 1, 2023
git("bzip2.git", x) {
    x.repo = "https://gitlab.com/bzip2/bzip2.git";
    x.dest_dir = BZ2_ROOT;
    x.commit_id = "66c46b8c9436613fd81bc5d03f63a61933a4dcc3";
}

cxx_static("bz2", x) {
    x.pub.include_dirs = {BZ2_ROOT};
    x.include_dirs = {BZ2_ROOT};

    x.srcs = {
        BZ2_ROOT + "/blocksort.c",
        BZ2_ROOT + "/huffman.c",
        BZ2_ROOT + "/crctable.c",
        BZ2_ROOT + "/randtable.c",
        BZ2_ROOT + "/compress.c",
        BZ2_ROOT + "/decompress.c",
        BZ2_ROOT + "/bzlib.c"
    };

    x.defines = {"BZ2_STATICLIB"};
    if (x.cfg["os"] == "win")
        x.defines += {"BZ_LCCWIN32", "BZ_UNIX=0"};
    else
        x.defines += {"BZ_LCCWIN32=0", "BZ_UNIX"};
}

cxx_executable("bzip2_exe", x) {
    x.srcs = {BZ2_ROOT + "/bzip2.c"};
    if (x.cfg["os"] == "win")
        x.defines = {"BZ_LCCWIN32", "BZ_UNIX=0"};
    else
        x.defines = {"BZ_LCCWIN32=0", "BZ_UNIX"};
    x.add_dep(":bz2", cxx::private_dep);
}

alias("bzip2", x) {
    x.actual_label = ":bz2";
}

alias("bz2_static", x) {
    x.actual_label = ":bz2";
}
