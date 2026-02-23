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
struct ProtobufContext : private cgn::QuickDepContext {
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
    cgn::CGNPathArray include_dirs = {"."};

    // The .pb.cc and .pb.h file output dir
    // set to opt->get_out_prefix_cfg0() if empty.
    cgn::CGNPath lang_out;

    std::string protoc = "@third_party//protobuf:protoc";

    // --plugin=protoc-gen-grpc=<DefaultInfo.outputs[0]> --grpc_out=<GenDir>
    // variable like : "@third_party//grpc:protoc-gen-grpc-cpp-plugin"
    std::string grpc_plugin_label;
    // void load_grpc_plugin(const std::string &label);
    
    friend struct ProtobufInterpreter;
    ProtobufContext(cgn::CGNTargetOpt *opt)
    : cgn::QuickDepContext{opt}, name(opt->name), cfg(opt->cfg){}
};

struct ProtobufInterpreter
{
    using context_type = ProtobufContext;

    constexpr static cgn::ConstLabelGroup<2> preload_labels() {
        return {"@third_party//protobuf/proto.cgn.cc", 
                "@cgn.d//library/cxx/cxx.cgn.cc"};
    }

    PROTOC_CGN_API static void interpret(context_type &x);
};

#define protobuf(name, x) CGN_RULE_DEFINE(ProtobufInterpreter, name, x)