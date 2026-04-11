#include <cgn>

static std::string ZSTD_ROOT = "repo";

// Zstandard v1.5.6 - Chrome Edition
git("zstd.git", x) {
    x.repo = "https://github.com/facebook/zstd.git";
    x.dest_dir = ZSTD_ROOT;
    x.commit_id = "794ea1b0afca0f020f4e57b6732332231fb23c70";
}

cxx_static("zstd_static", x) {
    x.defines = {"XXH_NAMESPACE=ZSTD_", "ZSTD_MULTITHREAD", 
        "ZSTD_LEGACY_SUPPORT=1",
        "ZSTD_LEGACY_MULTITHREADED_API"};
    if (x.cfg["optimization"] == "debug")
        x.defines += {"DEBUGLEVEL=0"};
    else
        x.defines += {"NDEBUG"};

    if (x.cfg["os"] == "win" && x.cfg["cxx_toolchain"] != "llvm")
        x.defines += {"ZSTD_HEAPMODE=0"};
    
    if (x.cfg["cxx_toolchain"] == "msvc" || x.cfg["cpu"] != "x86_64")
        x.defines += {"ZSTD_DISABLE_ASM"};
    else
        x.srcs += {ZSTD_ROOT + "/lib/decompress/*.S"};

    x.srcs += {ZSTD_ROOT + "/lib/common/*.c", ZSTD_ROOT + "/lib/compress/*.c", 
              ZSTD_ROOT + "/lib/decompress/*.c", 
              ZSTD_ROOT + "/lib/dictBuilder/*.c", ZSTD_ROOT + "/lib/legacy/*.c"};
              
    x.pub.include_dirs = {
        ZSTD_ROOT + "/lib",
        ZSTD_ROOT + "/lib/common",
        ZSTD_ROOT + "/lib/compress",
        ZSTD_ROOT + "/lib/decompress",
        ZSTD_ROOT + "/lib/dictBuilder"
    };
}

cxx_executable("zstd_exe", x) {
    x.defines = {"ZSTD_GZCOMPRESS", "ZSTD_GZDECOMPRESS"};
    x.perferred_binary_name = std::string{"zstd"} + (x.cfg["os"] == "win"?".exe":"");
    x.srcs = {
        ZSTD_ROOT + "/programs/benchfn.c",
        ZSTD_ROOT + "/programs/benchzstd.c",
        ZSTD_ROOT + "/programs/datagen.c",
        ZSTD_ROOT + "/programs/dibio.c",
        ZSTD_ROOT + "/programs/fileio_asyncio.c",
        ZSTD_ROOT + "/programs/fileio.c",
        ZSTD_ROOT + "/programs/lorem.c",
        ZSTD_ROOT + "/programs/timefn.c",
        ZSTD_ROOT + "/programs/util.c",
        ZSTD_ROOT + "/programs/zstdcli.c",
        ZSTD_ROOT + "/programs/zstdcli_trace.c"
    };

    x.add_dep("@third_party//zlib:z", cxx::private_dep);
    x.add_dep(":zstd_static", cxx::private_dep);
}

alias("zstd", x) { x.actual_label = ":zstd_static"; }
