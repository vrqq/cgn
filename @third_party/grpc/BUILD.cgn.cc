#include "grpc_src.h"
#include "common.cgn.h"
#include "third_party.cgn.h"
#include "@third_party/protobuf/proto.cgn.h"
#include <cgn>

// v1.65.2
git("grpc.git", x) {
    x.repo = "https://github.com/grpc/grpc.git";
    x.commit_id = "0bbbfd3c1593778d1429b55ef389e4cd78edb4f2";
    x.dest_dir = repo_dir;
}

// common source code
// ------------------
cxx_sources("grpc_impl_buildarg", x) {
    x.pub.cflags = {
        "-Wno-implicit-fallthrough",
        "-Wno-missing-field-initializers",
        "-Wno-unused-parameter"
    };
    x.pub.include_dirs = {repo_dir + "/include", repo_dir};
    x.add_dep(":libgrpc_third_party_xxhash", cxx::inherit);
    x.add_dep("@third_party//abseil-cpp", cxx::inherit);
    x.add_dep("@third_party//re2",    cxx::inherit);
    x.add_dep("@third_party//c-ares", cxx::inherit);
    x.add_dep("@third_party//zlib",   cxx::inherit);
    x.add_dep("@third_party//protobuf:libprotobuf", cxx::inherit);
}

cxx_static("libgrpc++_common", x) {
    x.srcs = add_prefix(repo_dir + "/", GRPC_COMMON_SRCS);
    x.add_dep(":grpc_impl_buildarg",      cxx::private_dep);
    x.add_dep(":libgrpc_upb_protos",      cxx::private_dep);
    x.add_dep(":libgrpc_third_party_upb", cxx::private_dep);
    x.add_dep(":libgrpc_third_party_utf8_range", cxx::private_dep);
    x.add_dep(":libgrpc_third_party_libaddress_sorting", cxx::private_dep);
}

// protoc grpc plugin
// ------------------
cxx_sources("libgrpc_plugin_support", x) {
    x.srcs = add_prefix(repo_dir + "/", {
        "src/compiler/cpp_generator.cc",
        "src/compiler/proto_parser_helper.cc",
        "src/compiler/python_generator.cc"
    });
    x.add_dep(":grpc_impl_buildarg", cxx::inherit);
    x.add_dep("@third_party//protobuf:libprotoc", cxx::inherit);
}
cxx_executable("protoc-gen-grpc-cpp-plugin", x) {
    x.pub.include_dirs = {repo_dir + "/include"};
    x.srcs = add_prefix(repo_dir + "/", {"src/compiler/cpp_plugin.cc"});
    x.add_dep(":libgrpc_plugin_support", cxx::private_dep);
}
cxx_executable("protoc-gen-grpc-python-plugin", x) {
    x.srcs = add_prefix(repo_dir + "/", {"src/compiler/python_plugin.cc"});
    x.add_dep(":libgrpc_plugin_support", cxx::private_dep);
}

// libgrpc++ reflection
// --------------------
// argfile:
//   @third_party/grpc/repo/src/proto/grpc/reflection/v1/reflection.proto
//   @third_party/grpc/repo/src/proto/grpc/reflection/v1alpha/reflection.proto
//   -I@third_party/grpc/repo
//   -I@third_party/protobuf/repo/src
//   --cpp_out=cgn-out/obj/@third_party_/grpc_/reflection_proto_FFFFBB1A/langsrc/
//   --plugin=protoc-gen-grpc=cgn-out/obj/@third_party_/grpc_/protoc-gen-grpc-cpp-plugin_FFFFBB1A/protoc-gen-grpc-cpp-plugin
//   --grpc_out=cgn-out/obj/@third_party_/grpc_/reflection_proto_FFFFBB1A/langsrc/
protobuf("reflection_proto", x) {
    x.include_dirs = {repo_dir};
    x.lang = x.Cxx;
    x.srcs = add_prefix(repo_dir + "/", {
        "src/proto/grpc/reflection/v1/reflection.proto",
        "src/proto/grpc/reflection/v1alpha/reflection.proto"
    });
    x.grpc_plugin_label = ":protoc-gen-grpc-cpp-plugin";
}
cxx_static("libgrpc++_reflection", x) {
    x.srcs = add_prefix(repo_dir + "/", {
        "src/cpp/ext/proto_server_reflection.cc",
        "src/cpp/ext/proto_server_reflection_plugin.cc"
    });
    x.add_dep(":reflection_proto", cxx::inherit);
    x.add_dep(":grpc_impl_buildarg", cxx::private_dep);
    x.add_dep(":libgrpc++_common", cxx::private_dep);
}

// gRPC C++ library target with no encryption or authentication
// ------------------------------------------------------------
cxx_static("libgrpc++_unsecure", x) {
    x.pub.include_dirs = {repo_dir + "/include"};
    x.srcs = add_prefix(repo_dir + "/", GRPC_UNSECURE_SRCS);
    x.add_dep(":grpc_impl_buildarg", cxx::private_dep);
    x.add_dep(":libgrpc_third_party_upb", cxx::private_dep);
    x.add_dep(":libgrpc_third_party_utf8_range", cxx::private_dep);
    x.add_dep(":libgrpc_upb_protos", cxx::private_dep);
    x.add_dep(":libgrpc++_common", cxx::private_dep);
}

// Secured gRPC C++ library target
cxx_static("libgrpc++", x) {
    x.pub.include_dirs = {repo_dir + "/include"};
    x.srcs = add_prefix(repo_dir + "/", GRPC_SECURE_SRCS);
    x.add_dep(":grpc_impl_buildarg", cxx::private_dep);
    x.add_dep(":libgrpc_third_party_upb", cxx::private_dep);
    x.add_dep(":libgrpc_third_party_utf8_range", cxx::private_dep);
    x.add_dep(":libgrpc_upb_protos", cxx::private_dep);
    x.add_dep(":libgrpc++_common", cxx::private_dep);
}

// third_party plugin
cxx_static("libgrpc_upb_protos", x) {
    x.include_dirs = x.pub.include_dirs = add_prefix(repo_dir + "/",{
        "src/core/ext/upb-gen",
        "src/core/ext/upbdefs-gen"
    });
    x.srcs = add_prefix(repo_dir + "/", {
        "src/core/ext/upb-gen/**.c",
        "src/core/ext/upbdefs-gen/**.c"
    });
    x.add_dep(":libgrpc_third_party_upb_headers", cxx::private_dep);
}


// target entry
alias("grpc", x) {
    x.actual_label = ":libgrpc++";
}
