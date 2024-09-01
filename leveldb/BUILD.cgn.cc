#include <cgn>

git("leveldb.git", x) {
    x.repo = "https://github.com/google/leveldb.git";
    x.commit_id = "068d5ee1a3ac40dabd00d211d5013af44be55bea";
    x.dest_dir = "repo";
}

cxx_static("leveldb", x) {
    // x.pub.defines = {"LEVELDB_SHARED_LIBRARY"};
    x.pub.include_dirs = {"repo/include", "repo/port"};
    x.include_dirs = {"repo", "repo/include"};

    x.srcs = {
        "repo/db/builder.cc",
        "repo/db/c.cc",
        "repo/db/db_impl.cc",
        "repo/db/db_iter.cc",
        "repo/db/dbformat.cc",
        "repo/db/dumpfile.cc",
        "repo/db/filename.cc",
        "repo/db/log_reader.cc",
        "repo/db/log_writer.cc",
        "repo/db/memtable.cc",
        "repo/db/repair.cc",
        "repo/db/table_cache.cc",
        "repo/db/version_edit.cc",
        "repo/db/version_set.cc",
        "repo/db/write_batch.cc",
        "repo/table/block_builder.cc",
        "repo/table/block.cc",
        "repo/table/filter_block.cc",
        "repo/table/format.cc",
        "repo/table/iterator.cc",
        "repo/table/merger.cc",
        "repo/table/table_builder.cc",
        "repo/table/table.cc",
        "repo/table/two_level_iterator.cc",
        "repo/util/arena.cc",
        "repo/util/bloom.cc",
        "repo/util/cache.cc",
        "repo/util/coding.cc",
        "repo/util/comparator.cc",
        "repo/util/crc32c.cc",
        "repo/util/env.cc",
        "repo/util/filter_policy.cc",
        "repo/util/hash.cc",
        "repo/util/logging.cc",
        "repo/util/options.cc",
        "repo/util/status.cc",
        "repo/helpers/memenv/memenv.cc"
    };
    x.defines = {"LEVELDB_COMPILE_LIBRARY"};
    if (x.cfg["os"] == "win") {
        x.defines += {"_UNICODE", "UNICODE",
                      "LEVELDB_PLATFORM_WINDOWS=1",
                    //   "_CRT_NONSTDC_NO_DEPRECATE",
                      "strdup=_strdup",
                    //   "_HAS_EXCEPTIONS=0",
        };
        x.srcs += {"repo/util/env_windows.cc"};
    }
    if (x.cfg["os"] == "linux") {
        x.defines += {"LEVELDB_PLATFORM_POSIX=1"};
        x.srcs += {"repo/util/env_posix.cc"};
    }

    x.add_dep("@third_party//zstd", cxx::private_dep);
} //factory "leveldb"

cxx_executable("leveldbutil", x) {
    x.srcs = {"repo/db/leveldbutil.cc"};
    x.add_dep(":leveldb", x.cfg, cxx::private_dep);
}

static void leveldb_benchmark_init(cxx::CxxContext &x) {
    if (x.cfg["os"] == "win") 
        x.defines += {"LEVELDB_PLATFORM_WINDOWS=1"};
    if (x.cfg["os"] == "linux") 
        x.defines += {"LEVELDB_PLATFORM_POSIX=1"};
    x.defines += {"LEVELDB_HAS_PORT_CONFIG_H=1"};
    x.include_dirs = {"repo", "tmp"};
    x.srcs += {"repo/util/histogram.cc", "repo/util/testutil.cc"};
    x.add_dep(":leveldb", x.cfg, cxx::private_dep);
    x.add_dep("@third_party//zstd", x.cfg, cxx::private_dep);
}

// TODO: using script to process #cmakedefine01
//
cxx_executable("leveldbbench", x) {
    leveldb_benchmark_init(x);
    x.srcs += {"repo/benchmarks/db_bench.cc"};
    x.add_dep("@third_party//googletest:gmock", cxx::private_dep);
}
