#include <cgn>

static std::string ZSTD_ROOT = "repo";

git("zstd.git", x) {
    x.repo = "https://github.com/facebook/zstd.git";
    x.dest_dir = ZSTD_ROOT;
    x.commit_id = "794ea1b0afca0f020f4e57b6732332231fb23c70";
}

cxx_static("zstd", x) {
    x.defines = {"XXH_NAMESPACE=ZSTD_", "ZSTD_MULTITHREAD", 
        "ZSTD_LEGACY_SUPPORT=1",
        "ZSTD_LEGACY_MULTITHREADED_API"};
    if (x.cfg["optimization"] == "debug")
        x.defines += {"DEBUGLEVEL=0"};
    else
        x.defines += {"NDEBUG"};
    if (x.cfg["os"] == "win" && x.cfg["toolchain"] != "llvm")
        x.defines += {"ZSTD_HEAPMODE=0"};
    
    x.srcs = {ZSTD_ROOT + "/lib/common/*.c", ZSTD_ROOT + "/lib/compress/*.c", 
              ZSTD_ROOT + "/lib/decompress/*.c", ZSTD_ROOT + "/lib/decompress/*.S", 
              ZSTD_ROOT + "/lib/dictBuilder/*.c", ZSTD_ROOT + "/lib/legacy/*.c"};
              
    x.pub.include_dirs = {
        ZSTD_ROOT + "/lib",
        ZSTD_ROOT + "/lib/common",
        ZSTD_ROOT + "/lib/compress",
        ZSTD_ROOT + "/lib/decompress",
        ZSTD_ROOT + "/lib/dictBuilder"
    };
}
