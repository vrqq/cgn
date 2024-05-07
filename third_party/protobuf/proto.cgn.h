// Protobuf interpreter
// see also : https://bazel.build/reference/be/protocol-buffer
//
#pragma once
#include <cgn>

// when lang == Cxx, the TargetInfos made by cxx_sources() would be returned.
struct ProtobufContext {
    // 'c':cxx  'r':rust  'p':python  'j':java
    enum Lang {
        Cxx, Rust, Python, Java
    }lang;
    
    std::vector<std::string> srcs;

    // keep empty: cgn-out/obj/...
    // not empty: relpath of current folder
    std::string lang_src_output;

    // The prefix to add to the paths of the .proto files in this rule.
    // When set, the .proto source files in the srcs attribute of this rule 
    // are accessible at is the value of this attribute prepended to their 
    // repository-relative path.
    // The prefix in the strip_import_prefix attribute is removed before this 
    // prefix is added.
    std::string import_prefix;

    // The prefix to strip from the paths of the .proto files in this rule.
    // When set, .proto source files in the srcs attribute of this rule are 
    // accessible at their path with this prefix cut off.
    // If it's a relative path (not starting with a slash), it's taken as a 
    // package-relative one. If it's an absolute one, it's understood as a 
    // repository-relative path.
    // The prefix in the import_prefix attribute is added after this prefix 
    // is stripped.
    std::string strip_import_prefix = "/";

    std::string protoc = "@third_party//protobuf:protoc";
};

struct ProtobufInterpreter
{
    using context_type = ProtobufContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@third_party//protobuf/proto.cgn.h"};
    }

    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
};
