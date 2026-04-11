#include <cgn>
#include "file_list.cgn.h"

// v33.5
git("protobuf.git", x) {
    x.repo = "https://github.com/protocolbuffers/protobuf.git";
    x.commit_id = "b6f9284da830b69be787732ffdaa35049d20a088";
    x.dest_dir = "repo";
}

namespace { //protobuf/build_defs/cpp_opts.bzl

void config_target(cxx::CxxContext &x) {
    if (x.cfg["cxx_toolchain"] == "msvc")
        x.cflags = {
            "/wd4065",  // switch statement contains 'default' but no 'case' labels
            "/wd4116",  // unnamed type definition in parentheses
            "/wd4146",  // unary minus operator applied to unsigned type
            "/wd4175",  // not all control paths return a value
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
            "/utf-8",   // Set source and execution character sets to UTF-8
            "/bigobj"   // Allow big object
        };
    
    if ((x.cfg["cxx_toolchain"]=="llvm" || x.cfg["cxx_toolchain"]=="gcc") && x.cfg["os"]=="linux") {
        if (x.cfg["optimization"] == "release")
            x.cflags = {
                "-Wno-sign-compare",
                "-Wno-nonnull",
                "-Wno-missing-field-initializers",
                "-Wno-overloaded-virtual",
                "-Wno-attributes",
                "-Wno-deprecated-declarations",
            };
        else
            x.cflags = {
                "-Woverloaded-virtual",
                "-Wno-sign-compare",
                "-Wno-nonnull",
                "-Wno-missing-field-initializers",
                "-Wno-deprecated-declarations"
            };
    }
    
    x.defines = {"HAVE_ZLIB", "GOOGLE_PROTOBUF_CMAKE_BUILD"};

    if (x.role == 'x') {
        if (x.cfg["cxx_toolchain"] == "msvc")
            x.ldflags = {
                "-ignore:4221",
                "Shell32.lib"
            };
        else if (x.cfg["os"] == "mac")
            x.ldflags = {
                "-lpthread",
                "-lm",
                "-framework CoreFoundation"
            };
        else
            x.ldflags = {
                "-lpthread",
                "-lm"
            };
    }
}

std::vector<std::string> add_prefix(std::vector<std::string> in, std::string prefix)
{
    for (auto &it : in)
        it = prefix + it;
    return in;
}

} //namespace<>

// dir : repo/third_party/utf8_range
// ---------------------------------

cxx_sources("utf8_range", x) {
    x.pub.include_dirs = {"repo/third_party/utf8_range"};
    x.srcs = {"repo/third_party/utf8_range/utf8_range.c"};
}

cxx_static("utf8_validity", x) {
    x.pub.include_dirs = {"repo/third_party/utf8_range"};
    x.add_dep(":utf8_range", cxx::inherit);
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
}

// repo/cmake/libupb.cmake
// dir : repo/upb
// --------------
cxx_static("upb", x) {
    x.pub.include_dirs = x.include_dirs = {"repo/upb/reflection/cmake", "repo"};
    x.srcs = add_prefix(libupb_srcs, "repo/")
           + add_prefix({"google/protobuf/descriptor.upb_minitable.c"}, "repo/upb/reflection/cmake/");

    config_target(x);
    x.add_dep(":utf8_range", cxx::private_dep);
}

// repo/cmake/upb_generators.cmake
// -------------------------------

cxx_executable("protoc-gen-upb", x) {
    x.include_dirs = {"repo/src", "repo/upb_generator/cmake"};
    x.srcs = add_prefix(protoc_gen_upb_srcs, "repo/")
           + add_prefix({"google/protobuf/compiler/plugin.upb_minitable.c"}, "repo/upb_generator/cmake/");
    x.add_dep(":upb", cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp", cxx::private_dep);
}

cxx_executable("protoc-gen-upbdefs", x) {
    x.include_dirs = {"repo/src", "repo/upb_generator/cmake"};
    x.srcs = add_prefix(protoc_gen_upbdefs_srcs, "repo/")
           + add_prefix({"google/protobuf/compiler/plugin.upb_minitable.c"}, "repo/upb_generator/cmake/");
    x.add_dep(":upb", cxx::private_dep);
    x.add_dep(":utf8_validity", cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp", cxx::private_dep);
}


cxx_static("protoc_static", x) {
    if (x.cfg["os"] == "win")
        x.perferred_binary_name = "protoc.lib";
    else
        x.perferred_binary_name = "libprotoc.a";

    x.include_dirs = {"repo/src", "repo"};
    x.pub.include_dirs = {"repo/src"};
    config_target(x);
    x.srcs = add_prefix(libprotoc_srcs, "repo/");
    x.add_dep(":protobuf_static", cxx::private_dep);
    x.add_dep(":upb", cxx::inherit);
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
}

cxx_executable("protoc", x) {
    x.include_dirs = {"repo/src", "repo"};
    x.srcs = {"repo/src/google/protobuf/compiler/main.cc"};
    config_target(x);
    x.add_dep(":protoc_static", cxx::private_dep);
    x.add_dep(":protobuf_static", cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp", cxx::private_dep);
}


alias("host_compiler", x) {
    x.actual_label = ":protoc";
    x.load_named_config("host_release");
}

cxx_static("protobuf-lite_static", x) {
    if (x.cfg["os"] == "win")
        x.perferred_binary_name = "protobuf-lite.lib";
    else
        x.perferred_binary_name = "libprotobuf-lite.a";

    x.include_dirs = {"repo/src", "repo"};
    x.pub.include_dirs = {"repo/src"};
    config_target(x);
    x.srcs = add_prefix(libprotobuf_lite_srcs, "repo/");
    x.add_dep(":utf8_validity", cxx::inherit);
    x.add_dep("@third_party//zlib", cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
}

cxx_static("protobuf_static", x) {
    if (x.cfg["os"] == "win")
        x.perferred_binary_name = "protobuf.lib";
    else
        x.perferred_binary_name = "libprotobuf.a";

    x.include_dirs = {"repo/src", "repo"};
    x.pub.include_dirs = {"repo/src"};
    if (x.cfg["cxx_toolchain"] == "gcc" || x.cfg["cxx_toolchain"] == "llvm")
        x.pub.cflags += {"-Wno-deprecated-declarations"};
    config_target(x);
    x.srcs = add_prefix(libprotobuf_srcs, "repo/");
    x.add_dep(":utf8_validity", cxx::inherit);
    x.add_dep("@third_party//zlib", cxx::private_dep);
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
}

alias("protobuf", x) {
    x.actual_label = ":protobuf_static";
}

//TODO: 当前问题
//   1) 由 cmake_config 生成的文件 对libname命名不一 例如cmake认为是 "libprotobuf-lited.a"
//   2) 在grpc的 cmake文件中 强行以 find_package(protobuf CONFIG) 模式搜索
//      故只能生成 protobuf-Config.cmake 供其使用, ".pc" (pkg-config) 不能用
//      也不能用 find_package(MODULE) 模式, 
//      或许grpc认为 protobuf的cmake是自己写的 自己认得, README中也不考虑由 apt 安装的
//
//可能的方案: 
//  * 手动改 .cmake 别的工程可能也能使
//    由于 brpc内部使用 find_package(Protobuf REQUIRED) 此方案brpc并不受益
//  * 改grpc到本地target
//    似乎也没有想象的复杂
//
cmake_config("cmake_config", x) {
    auto *absl = x.add_dep("@third_party//abseil-cpp").get<BinDevelInfo>();
    x.sources_dir = "repo";
    x.vars["protobuf_BUILD_TESTS"] = "OFF";
    x.vars["protobuf_BUILD_SHARED_LIBS"] = "OFF";
    x.vars["protobuf_ABSL_PROVIDER"] = "package";
    x.vars["absl_ROOT"] = absl->install_dir;
    x.outputs = {
        "cmake/protobuf/protobuf-config-version.cmake",
        "cmake/protobuf/protobuf-config.cmake",
        "cmake/protobuf/protobuf-generate.cmake",
        "cmake/protobuf/protobuf-module.cmake",
        "cmake/protobuf/protobuf-options.cmake",
        "CMakeFiles/Export/e09086e7eadf5835f8bdfa7f95657dc2/protobuf-targets.cmake"
    };

    // TODO: let filegroup() to collect "**.cmake" in flat mode
    //       then call it via bin_devel()
    x.outputs += {std::string{
        "CMakeFiles/Export/e09086e7eadf5835f8bdfa7f95657dc2/protobuf-targets-"
        } + (x.cfg["optimization"] == "debug"?"debug.cmake":"noconfig.cmake")
    };
}

// bin_devel("devel", x) {
//     x.include = {
//         {"repo/third_party/utf8_range", {"*.h"}},
//         {"repo/src", {"google/protobuf/**.h"}}
//     };
//     x.add_from_target(":libprotobuf", upbx.allow_default);
//     x.add_from_target(":libprotoc",   x.allow_default);
//     // x.add_cmake_config_from_target(":cmake_config", "protobuf");
//     // x.gen_pkgconfig_from_target(":libprotobuf", //label
//     //     "protobuf", //"Protocol Buffers",  //Name
//     //     "Google's Data Interchange Format", //Description
//     //     //Requires
//     //     "absl_absl_check absl_absl_log absl_algorithm absl_base absl_bind_front "
//     //     "absl_bits absl_btree absl_cleanup absl_cord absl_core_headers "
//     //     "absl_debugging absl_die_if_null absl_dynamic_annotations absl_flags "
//     //     "absl_flat_hash_map absl_flat_hash_set absl_function_ref absl_hash "
//     //     "absl_if_constexpr absl_layout absl_log_initialize absl_log_severity "
//     //     "absl_memory absl_node_hash_map absl_node_hash_set absl_optional "
//     //     "absl_span absl_status absl_statusor absl_strings absl_synchronization "
//     //     "absl_time absl_type_traits absl_utility absl_variant utf8_range",
//     //     "26.1.0" //Version
//     // );
// }
