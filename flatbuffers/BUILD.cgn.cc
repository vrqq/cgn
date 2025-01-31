#include <cgn>
// #include "flatbuf_srclist.cgn.h"

static const std::string repo = "repo";

// FlatBuffers Version 25.1.24
git("flatbuffers.git", x) {
    x.repo = "https://github.com/google/flatbuffers.git";
    x.commit_id = "0312061985dbaaf6b068006383946ac6095f5b63";
    x.dest_dir = repo;
}

// Build arguments copied from Chromium source code
// std::vector<std::string flatbuffer_srcs = {
//   repo + "src/idl_parser.cpp",
//   repo + "src/idl_gen_text.cpp",
//   repo + "src/reflection.cpp",
//   repo + "src/util.cpp",
// };

std::vector<std::string> libflatbuffers_src = {
    repo + "/grpc/src/compiler/cpp_generator.cc",
    repo + "/grpc/src/compiler/cpp_generator.h",
    repo + "/grpc/src/compiler/go_generator.cc",
    repo + "/grpc/src/compiler/go_generator.h",
    repo + "/grpc/src/compiler/java_generator.cc",
    repo + "/grpc/src/compiler/java_generator.h",
    repo + "/grpc/src/compiler/python_generator.cc",
    repo + "/grpc/src/compiler/python_generator.h",
    repo + "/grpc/src/compiler/schema_interface.h",
    repo + "/grpc/src/compiler/swift_generator.cc",
    repo + "/grpc/src/compiler/swift_generator.h",
    repo + "/grpc/src/compiler/ts_generator.cc",
    repo + "/grpc/src/compiler/ts_generator.h",
    repo + "/include/codegen/python.cc",
    repo + "/include/codegen/python.h",
    repo + "/include/flatbuffers/code_generators.h",
    repo + "/include/flatbuffers/flatc.h",
    repo + "/include/flatbuffers/grpc.h",
    repo + "/include/flatbuffers/hash.h",
    repo + "/include/flatbuffers/idl.h",
    repo + "/include/flatbuffers/minireflect.h",
    repo + "/include/flatbuffers/reflection.h",
    repo + "/include/flatbuffers/reflection_generated.h",
    repo + "/include/flatbuffers/registry.h",
    repo + "/src/annotated_binary_text_gen.cpp",
    repo + "/src/annotated_binary_text_gen.h",
    repo + "/src/bfbs_gen.h",
    repo + "/src/bfbs_gen_lua.cpp",
    repo + "/src/bfbs_gen_lua.h",
    repo + "/src/bfbs_gen_nim.cpp",
    repo + "/src/bfbs_gen_nim.h",
    repo + "/src/bfbs_namer.h",
    repo + "/src/binary_annotator.cpp",
    repo + "/src/binary_annotator.h",
    repo + "/src/code_generators.cpp",
    repo + "/src/flatc.cpp",
    repo + "/src/idl_gen_binary.cpp",
    repo + "/src/idl_gen_binary.h",
    repo + "/src/idl_gen_cpp.cpp",
    repo + "/src/idl_gen_cpp.h",
    repo + "/src/idl_gen_csharp.cpp",
    repo + "/src/idl_gen_csharp.h",
    repo + "/src/idl_gen_dart.cpp",
    repo + "/src/idl_gen_dart.h",
    repo + "/src/idl_gen_fbs.cpp",
    repo + "/src/idl_gen_go.cpp",
    repo + "/src/idl_gen_go.h",
    repo + "/src/idl_gen_grpc.cpp",
    repo + "/src/idl_gen_java.cpp",
    repo + "/src/idl_gen_java.h",
    repo + "/src/idl_gen_json_schema.cpp",
    repo + "/src/idl_gen_json_schema.h",
    repo + "/src/idl_gen_kotlin.cpp",
    repo + "/src/idl_gen_kotlin.h",
    repo + "/src/idl_gen_kotlin_kmp.cpp",
    repo + "/src/idl_gen_lobster.cpp",
    repo + "/src/idl_gen_lobster.h",
    repo + "/src/idl_gen_php.cpp",
    repo + "/src/idl_gen_php.h",
    repo + "/src/idl_gen_python.cpp",
    repo + "/src/idl_gen_python.h",
    repo + "/src/idl_gen_rust.cpp",
    repo + "/src/idl_gen_rust.h",
    repo + "/src/idl_gen_swift.cpp",
    repo + "/src/idl_gen_swift.h",
    repo + "/src/idl_gen_text.cpp",
    repo + "/src/idl_gen_text.h",
    repo + "/src/idl_gen_ts.cpp",
    repo + "/src/idl_gen_ts.h",
    repo + "/src/idl_namer.h",
    repo + "/src/idl_parser.cpp",
    repo + "/src/namer.h",
    repo + "/src/reflection.cpp",
    repo + "/src/util.cpp",
};

std::vector<std::string> grpc_internal_includes = {
    repo + "/include",
    repo + "/src",
    repo,
    repo + "/generated",
    repo + "/grpc",
};
// static std::vector<std::string> 
// add_prefix(const std::string &prefix, std::vector<std::string> in) {
//     for (auto &it : in)
//         it = prefix + it;
//     return in;
// }

cxx_static("libflatbuffers", x) {
    x.defines = {"FLATBUFFERS_LOCALE_INDEPENDENT=0"};
    x.pub.include_dirs = {repo + "/include"};
    x.include_dirs = grpc_internal_includes;
    x.srcs = libflatbuffers_src;
    
    if (x.cfg["os"] == "win")
        x.perferred_binary_name = "flatbuffers.lib";
    else
        x.perferred_binary_name = "libflatbuffers.a";
    
    if (x.cfg["toolchain"] == "llvm") {
        x.cflags += {
            "-Wno-constant-conversion",
            "-Wno-shorten-64-to-32",
        };
    }
}

cxx_executable("flatc", x) {
    if (x.cfg["os"] == "linux")
        x.cflags = {
            "-Wno-implicit-fallthrough", // in reflection.cpp
        };
    x.defines = {"FLATBUFFERS_LOCALE_INDEPENDENT=0"};
    x.include_dirs = grpc_internal_includes;
    x.srcs = {repo + "/src/flatc_main.cpp"};
    x.add_dep(":libflatbuffers", cxx::private_dep);
}

alias("flatbuffers", x) {
    x.actual_label = ":libflatbuffers";
}
