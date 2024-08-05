#include <cgn>
#include "file_list.cgn.h"

// v26.1
git("protobuf.git", x) {
    x.repo = "https://github.com/protocolbuffers/protobuf.git";
    x.commit_id = "2434ef2adf0c74149b9d547ac5fb545a1ff8b6b5";
    x.dest_dir = "repo";
}

namespace { //protobuf/build_defs/cpp_opts.bzl

std::vector<std::string> COPTS(const cgn::Configuration &cfg) {
    if (cfg["toolchain"] == "msvc")
        return {
            "/wd4065",  // switch statement contains 'default' but no 'case' labels
            "/wd4146",  // unary minus operator applied to unsigned type
            "/wd4244",  // 'conversion' conversion from 'type1' to 'type2', possible loss of data
            "/wd4251",  // 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
            "/wd4267",  // 'var' : conversion from 'size_t' to 'type', possible loss of data
            "/wd4305",  // 'identifier' : truncation from 'type1' to 'type2'
            "/wd4307",  // 'operator' : integral constant overflow
            "/wd4309",  // 'conversion' : truncation of constant value
            "/wd4334",  // 'operator' : result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
            "/wd4355",  // 'this' : used in base member initializer list
            "/wd4506",  // no definition for inline function 'function'
            "/wd4800",  // 'type' : forcing value to bool 'true' or 'false' (performance warning)
            "/wd4996",  // The compiler encountered a deprecated declaration.
            "/utf-8"    // Set source and execution character sets to UTF-8
        };
    
    if (cfg["optimization"] == "release")
        return {
            "-DHAVE_ZLIB",
            "-Wno-sign-compare",
            "-Wno-nonnull",
            "-Wno-missing-field-initializers"
            "-Wno-overloaded-virtual",
            "-Wno-attributes",
        };
    return {
        "-DHAVE_ZLIB",
        "-Woverloaded-virtual",
        "-Wno-sign-compare",
        "-Wno-nonnull",
        "-Wno-missing-field-initializers"
    };
};

std::vector<std::string> LINK_OPTS(const cgn::Configuration &cfg) {
    if (cfg["toolchain"] == "msvc")
        return {
            "-ignore:4221",
            "Shell32.lib"
        };
    if (cfg["os"] == "mac")
        return {
            "-lpthread",
            "-lm",
            "-framework CoreFoundation"
        };
    return {
        "-lpthread",
        "-lm"
    };
}

std::vector<std::string> add_prefix(std::vector<std::string> in, std::string prefix)
{
    for (auto &it : in)
        it = prefix + it;
    return in;
}


} //namespace<>

cxx_static("libprotoc", x) {
    x.include_dirs = {"repo/src", "repo"};
    x.cflags  = COPTS(x.cfg);
    x.srcs = add_prefix(libprotoc_srcs, "repo");
    x.add_dep(":libprotobuf", cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
}

cxx_executable("protoc", x) {
    x.include_dirs = {"repo/src", "repo"};
    x.srcs = {"repo/src/google/protobuf/compiler/main.cc"};
    x.ldflags = LINK_OPTS(x.cfg);
    x.add_dep(":libprotoc", cxx::private_dep);
    x.add_dep(":libprotobuf", cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp", cxx::private_dep);
}

cxx_static("libprotobuf-lite", x) {
    x.include_dirs = {"repo/src", "repo"};
    x.pub.include_dirs = {"repo/src"};
    x.cflags = COPTS(x.cfg);
    x.srcs = add_prefix(libprotobuf_lite_srcs, "repo");
    x.add_dep(":utf8_validity", cxx::inherit);
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
}

cxx_static("libprotobuf", x) {
    x.include_dirs = {"repo/src", "repo"};
    x.pub.include_dirs = {"repo/src"};
    x.cflags = COPTS(x.cfg);
    x.srcs = add_prefix(libprotobuf_srcs, "repo");
    x.add_dep(":utf8_validity", cxx::inherit);
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
}

cxx_sources("utf8_validity", x) {
    x.pub.include_dirs = {"repo/third_party/utf8_range"};
    x.srcs = {
        "repo/third_party/utf8_range/utf8_range.c",
        "repo/third_party/utf8_range/utf8_validity.cc",
    };
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
}