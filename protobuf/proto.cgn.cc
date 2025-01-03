// Subunit target output layout
// <out>/lang_pb/my_proto.pb.h  (if lang_out empty)
// <out>/lang_pb/bin/my_proto.o
#include <fstream>
#include "@cgn.d/library/cxx.cgn.bundle/cxx.cgn.h"
#include "proto.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

// BUGFIX:
//  since the .proto filepath may starting with '@', so we have to generate
//  a option file to `protoc @optfile`
// CMDLINE:
//  protoc -I<x.include_dirs>
//      --cpp_out=<x.cpp_out> 
//      --plugin=protoc-gen-grpc=<path(x.grpc_label)> --grpc_out=<x.cpp_out>
//      <x.srcs>
//
void ProtobufInterpreter::interpret(context_type &x)
{
    if (x.srcs.empty() || x.lang != x.Cxx) {
        x.opt->confirm_with_error("empty src or unsupported lang");
        return ;
    }

    // load predefined config["host_release"]
    // add adep to named-config
    auto host_cfg = api.query_config("host_release");
    if (host_cfg.second == nullptr) {
        x.opt->confirm_with_error("config 'host_release' not found");
        return ;
    }
    x.opt->quickdep_early_anodes += {host_cfg.second};

    // load protoc exe
    cgn::CGNTarget protoc = x.opt->quick_dep(x.protoc, host_cfg.first, false);
    if (protoc.errmsg.size()) {
        x.opt->confirm_with_error(protoc.errmsg);
        return ;
    }

    // load grpc plugin
    std::string grpc_plugin_exe;
    if (x.grpc_plugin_label.size()) {
        auto grpc = x.opt->quick_dep(x.grpc_plugin_label, host_cfg.first, false);
        if (grpc.errmsg.size()) {
            x.opt->confirm_with_error(grpc.errmsg);
            return ;
        }
        grpc_plugin_exe = grpc.outputs[0];
    }

    // confirm target opt
    cgn::CGNTargetOpt *pb_opt = x.opt->confirm();

    // rebase lang_out_dir
    if (x.lang_out.size())
        x.lang_out = api.locale_path(pb_opt->src_prefix + x.lang_out + "/");
    else
        x.lang_out = pb_opt->out_prefix + "cpp_out/";

    // ninja[protoc] arg file: .protorsp
    std::string proto_argfile = pb_opt->out_prefix + ".protorsp";
    std::vector<std::string> ninja_input = {proto_argfile};
    std::vector<std::string> ninja_out;
    std::vector<std::string> cppctx_in;  // args for cxx_context
    std::ofstream fout;

    // regenerate arg file if ninja file changed
    if (!pb_opt->file_unchanged) {
        fout.open(proto_argfile);

        // ninja[protoc] arg file: {-I...}
        for (auto it : x.include_dirs)
            fout<<"-I" + api.locale_path(pb_opt->src_prefix + it) + "\n";
        fout<<"-I" + api.get_filepath("@third_party//protobuf/repo/src") + "\n";
        // fout<<"-I" + api.get_filepath("@third_party//protobuf/repo/src") + "\n";

        // ninja[protoc] arg file: {--cpp_out=... --grpc_out=...}
        fout<<"--cpp_out=" + two_escape(x.lang_out)<<"\n";
        if (grpc_plugin_exe.size())
            fout<<"--plugin=protoc-gen-grpc=" + two_escape(grpc_plugin_exe)<<"\n"
                <<"--grpc_out=" + two_escape(x.lang_out)<<"\n";
    }


    for (auto it : x.srcs) {
        if (it.size() < 6 || it.substr(it.size()-6) != ".proto")
            continue;
        
        // Test and find the relevant path for the ".proto" file to 
        // any of "x.include_dir[]".
        // The out_stem is the path which .proto relavent to x.include_dir[]
        std::string out_stem;
        for (auto inc : x.include_dirs) {
            // "one of `inc`(x.include_dirs) must be an exact prefix of the `it`(x.srcs)"
            std::string reb = api.rebase_path(it, inc);
            if (reb[0] != '.' && reb[1] != '.') {
                out_stem = reb.substr(0, reb.size() - 6);
                break;
            }
        }
        if (out_stem.empty()) {
            pb_opt->result.errmsg = "Protoc: you must assign a include_dir "
                                 "which is the prefix of src file. ";
            return ;
        }

        std::string s1 = x.lang_out + out_stem;

        // ninja[protoc] arg file: {xx.proto}
        if (!pb_opt->cache_result_found) {
            std::string protofile = api.locale_path(pb_opt->src_prefix + it);
            fout<<protofile<<"\n";
            ninja_input += {protofile};

            ninja_out += {pb_opt->ninja->escape_path(s1 + ".pb.cc"), 
                          pb_opt->ninja->escape_path(s1 + ".pb.h")};
        }

        // cpp src in: <it>.pb.cc
        cppctx_in += {api.rebase_path(s1 + ".pb.cc", pb_opt->src_prefix)};
    } //end_for(x.srcs[])

    if (!pb_opt->cache_result_found) {
        // ninja[protoc] run
        pb_opt->ninja->append_include(
            api.get_filepath("@cgn.d//library/general.cgn.bundle/rule.ninja")
        );

        // *.proto => *.pb.h / *.pb.cc
        auto *field = pb_opt->ninja->append_build();
        field->rule = "run";
        field->inputs = {pb_opt->ninja->escape_path(protoc.outputs[0])};
        field->implicit_inputs = pb_opt->ninja->escape_path(ninja_input);
        field->variables["args"] = "@" + proto_argfile;
        field->variables["desc"] = "PROTOC " + pb_opt->factory_label;
        field->outputs = ninja_out;
    }

    // create sub-unit-target for CxxInterpreter {xx.pb.h, xx.pb.cc}
    // return via CxxInterpreter
    cgn::CGNTargetOptIn *cxx_optin = pb_opt->create_sub_target("cpp", true);
    cxx::CxxSourcesContext cxx_ctx(cxx_optin);
    cxx_ctx.include_dirs = cxx_ctx.pub.include_dirs 
                         = {api.rebase_path(x.lang_out, pb_opt->src_prefix)};
    cxx_ctx.srcs = cppctx_in;
    cxx_ctx.add_dep("@third_party//protobuf:libprotobuf", cxx::inherit);
    cxx::CxxInterpreter::interpret(cxx_ctx);

    // TODO
    // api.add_adep_edge(pb_opt->anode, cxx_optin->confirm()->anode);
}
