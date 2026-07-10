#include <cgn>
static const std::string repo = "repo";

// FlatBuffers v25.12.19-2026-02-06-03fffb2
git("flatbuffers.git", x) {
    x.repo = "https://github.com/google/flatbuffers.git";
    x.commit_id = "03fffb25e2d777462b719cb4964249c30b19d58f";
    x.dest_dir = repo;
}

// CMake: FlatBuffers_Library_SRCS
std::vector<std::string> flatbuffers_runtime_srcs = {
    repo + "/src/file_manager.cpp",
    repo + "/src/file_name_manager.cpp",
    repo + "/src/idl_parser.cpp",
    repo + "/src/idl_gen_text.cpp",
    repo + "/src/reflection.cpp",
    repo + "/src/util.cpp",
};

// CMake: FlatBuffers_Compiler_SRCS minus runtime sources and flatc_main.cpp
std::vector<std::string> flatc_library_srcs = {
    repo + "/grpc/src/compiler/cpp_generator.cc",
    repo + "/grpc/src/compiler/go_generator.cc",
    repo + "/grpc/src/compiler/java_generator.cc",
    repo + "/grpc/src/compiler/python_generator.cc",
    repo + "/grpc/src/compiler/swift_generator.cc",
    repo + "/grpc/src/compiler/ts_generator.cc",
    repo + "/include/codegen/python.cc",
    repo + "/src/annotated_binary_text_gen.cpp",
    repo + "/src/bfbs_gen_lua.cpp",
    repo + "/src/bfbs_gen_nim.cpp",
    repo + "/src/binary_annotator.cpp",
    repo + "/src/code_generators.cpp",
    repo + "/src/flatc.cpp",
    repo + "/src/idl_gen_binary.cpp",
    repo + "/src/idl_gen_cpp.cpp",
    repo + "/src/idl_gen_csharp.cpp",
    repo + "/src/idl_gen_dart.cpp",
    repo + "/src/idl_gen_fbs.cpp",
    repo + "/src/idl_gen_go.cpp",
    repo + "/src/idl_gen_grpc.cpp",
    repo + "/src/idl_gen_java.cpp",
    repo + "/src/idl_gen_json_schema.cpp",
    repo + "/src/idl_gen_kotlin.cpp",
    repo + "/src/idl_gen_kotlin_kmp.cpp",
    repo + "/src/idl_gen_lobster.cpp",
    repo + "/src/idl_gen_php.cpp",
    repo + "/src/idl_gen_python.cpp",
    repo + "/src/idl_gen_rust.cpp",
    repo + "/src/idl_gen_swift.cpp",
    repo + "/src/idl_gen_ts.cpp",
};

std::vector<std::string> flatbuffers_internal_includes = {
    repo + "/include",
    repo + "/src",
    repo,
    repo + "/generated",
    repo + "/grpc",
};

cxx_static("libflatbuffers", x) {
    x.defines = {"FLATBUFFERS_LOCALE_INDEPENDENT=0"};
    x.pub.include_dirs = {repo + "/include"};
    x.include_dirs = flatbuffers_internal_includes;
    x.srcs = flatbuffers_runtime_srcs;

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

cxx_static("libflatc", x) {
    x.defines = {"FLATBUFFERS_LOCALE_INDEPENDENT=0"};
    x.pub.include_dirs = {repo + "/include"};
    x.include_dirs = flatbuffers_internal_includes;
    x.srcs = flatc_library_srcs;
    x.add_dep(":libflatbuffers", cxx::private_dep);

    if (x.cfg["toolchain"] == "llvm") {
        x.cflags += {
            "-Wno-constant-conversion",
            "-Wno-shorten-64-to-32",
        };
    }
}

cxx_executable("flatc", x) {
    x.defines = {"FLATBUFFERS_LOCALE_INDEPENDENT=0"};
    x.include_dirs = flatbuffers_internal_includes;
    x.srcs = {repo + "/src/flatc_main.cpp"};
    x.add_dep(":libflatc", cxx::private_dep);

    if (x.cfg["os"] == "linux") {
        x.cflags += {
            "-Wno-implicit-fallthrough", // in reflection.cpp
        };
    }
}

alias("flatbuffers", x) {
    x.actual_label = ":libflatbuffers";
}
