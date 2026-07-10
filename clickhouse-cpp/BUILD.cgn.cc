#include <cgn>

git("clickhouse-cpp.git", x) {
    x.repo = "https://github.com/ClickHouse/clickhouse-cpp.git";
    x.commit_id = "001025cd72d904104c15657e85e0fb6b0ec58e14";
    x.dest_dir = "repo";
}


cxx_static("clickhouse-cpp", x) {
    auto add_prefix = [](cgn::CGNPathArray in) {
        for (auto &it : in)
            it.rpath = "repo/clickhouse/" + it.rpath;
        return in;
    };

    x.srcs = add_prefix({
        "base/compressed.cpp",
        "base/input.cpp",
        "base/output.cpp",
        "base/platform.cpp",
        "base/socket.cpp",
        "base/wire_format.cpp",
        "base/endpoints_iterator.cpp",

        "columns/array.cpp",
        "columns/bool.cpp",
        "columns/column.cpp",
        "columns/date.cpp",
        "columns/decimal.cpp",
        "columns/enum.cpp",
        "columns/factory.cpp",
        "columns/geo.cpp",
        "columns/ip4.cpp",
        "columns/ip6.cpp",
        "columns/json.cpp",
        "columns/lowcardinality.cpp",
        "columns/nullable.cpp",
        "columns/numeric.cpp",
        "columns/map.cpp",
        "columns/string.cpp",
        "columns/tuple.cpp",
        "columns/time.cpp",
        "columns/uuid.cpp",

        "columns/itemview.cpp",

        "types/type_parser.cpp",
        "types/types.cpp",

        "block.cpp",
        "client.cpp",
        "query.cpp"
    });
    x.include_dirs = x.pub.include_dirs = {"repo"};
    x.cflags = {"-Wno-deprecated-declarations"};

    x.add_dep(":cityhash", cxx::private_dep);
    x.add_dep("@third_party//zstd", cxx::private_dep);
    x.add_dep("@third_party//lz4",  cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp:int128", cxx::inherit);

    // WITH_OPENSSL : OFF in default
    // x.add_dep("@third_party//openssl", cxx::private_dep);
}

cxx_sources("cityhash", x) {
    x.srcs = {
        "repo/contrib/cityhash/cityhash/city.cc",
    };
    x.pub.include_dirs = {
        "repo/contrib/cityhash/cityhash"
    };
}

cxx_executable("bench", x) {
    x.srcs = {"repo/bench/bench.cpp"};
    x.include_dirs = {"repo"};
    x.add_dep(":clickhouse-cpp", cxx::private_dep);
}