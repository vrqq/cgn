// Protobuf interpreter
// see also : https://bazel.build/reference/be/protocol-buffer
//
#ifdef _WIN32
    #ifdef PROTOC_CGN_IMPL
        #define PROTOC_CGN_API  __declspec(dllexport)
    #else
        #define PROTOC_CGN_API
    #endif
#else
    #define PROTOC_CGN_API __attribute__((visibility("default")))
#endif

#pragma once
#include <cgn>

// when lang == Cxx, the TargetInfos made by cxx_sources() would be returned.
struct ProtobufContext {
    const std::string &name;
    const cgn::Configuration &cfg;

    // 'c':cxx  'r':rust  'p':python  'j':java
    enum Lang {
        UNDEFINED,
        Cxx, Rust, Python, Java
    }lang = UNDEFINED;
    
    std::vector<std::string> srcs;

    // args to "protoc -I"
    // Specify the directory in which to search for
    // imports.  May be specified multiple times;
    // directories will be searched in order.
    //
    // In other word, the 'import' keyword in .proto file would guide 
    // to search by this list in order, and for cpp_out it ensure the 
    // 'include' keyword in .pb.h and .pb.cc files maintains the same 
    // relative path. Commonly, the list is set to ["."].
    std::vector<std::string> include_dirs;

    // If empty:
    //  cgn-out/obj/<target_out>/lang_out
    // If not empty:
    //  relative-path of current folder
    std::string lang_out;

    // [depecrated: replaced by include_dirs]
    // The prefix to add to the paths of the .proto files in this rule.
    // When set, the .proto source files in the srcs attribute of this rule 
    // are accessible at is the value of this attribute prepended to their 
    // repository-relative path.
    // The prefix in the strip_import_prefix attribute is removed before this 
    // prefix is added.
    // std::string import_prefix;

    // [depecrated: replaced by include_dirs]
    // The prefix to strip from the paths of the .proto files in this rule.
    // When set, .proto source files in the srcs attribute of this rule are 
    // accessible at their path with this prefix cut off.
    // If it's a relative path (not starting with a slash), it's taken as a 
    // package-relative one. If it's an absolute one, it's understood as a 
    // repository-relative path.
    // The prefix in the import_prefix attribute is added after this prefix 
    // is stripped.
    // std::string strip_import_prefix = "/";

    std::string protoc = "@third_party//protobuf:protoc";

    // --plugin=protoc-gen-grpc=<DefaultInfo.outputs[0]> --grpc_out=<GenDir>
    // variable like : "@third_party//grpc:protoc-gen-grpc-cpp-plugin"
    std::string grpc_plugin_label;
    // void load_grpc_plugin(const std::string &label);

    ProtobufContext(cgn::CGNTargetOptIn *opt)
    : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

private: friend struct ProtobufInterpreter;
    cgn::CGNTargetOptIn *opt;
};

struct ProtobufInterpreter
{
    using context_type = ProtobufContext;

    constexpr static cgn::ConstLabelGroup<3> preload_labels() {
        return {"@third_party//protobuf/proto.cgn.cc", 
                "@cgn.d//library/cxx.cgn.bundle",
                "@cgn.d//library/general.cgn.bundle"};
    }

    PROTOC_CGN_API static void interpret(context_type &x);
};

#define protobuf(name, x) CGN_RULE_DEFINE(ProtobufInterpreter, name, x)