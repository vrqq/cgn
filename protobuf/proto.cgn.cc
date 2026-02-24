// Target output layput
//   ${get_out_prefix_cfg0}/${LANG}_pb/*.pb.cc
//   ${get_out_prefix_cfg0}/${LANG}_pb/*.pb.h
//   ${out_prefix}/${LANG}_bin/*.pb.o
#define PROTOC_CGN_IMPL
#include <fstream>
#include "@cgn.d/library/cxx/cxx.cgn.h"
#include "proto.cgn.h"

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
    if (x.srcs.empty() || x.lang != x.Cxx)
        return x.opt->set_fail("empty src, output or unsupported lang");

    // load predefined config["host_release"]
    // add adep to named-config
    cgn::CGNTarget protoc = x.quick_dep_namedcfg(x.protoc, "host_release", false);
    if (protoc.errmsg.size())
        return x.opt->set_fail("protoc: " + protoc.errmsg);

    // load grpc plugin
    std::string grpc_plugin_exe;
    if (x.grpc_plugin_label.size()) {
        auto grpc = x.quick_dep_namedcfg(x.grpc_plugin_label, "host_release", false);
        if (grpc.errmsg.size())
            return x.opt->set_fail("grpc_plugin: " + grpc.errmsg);
        if (grpc.outputs.empty())
            return x.opt->set_fail("grpc_plugin: no output");
        grpc_plugin_exe = grpc.outputs[0];
    }

    // confirm target opt
    // Current target result maybe include:
    // - CxxInfo.include_dirs[] = {$pb_basedir}
    // - LinkAndRunInfo.object_files = merge_from_cxx_target
    // - outputs[] = $pb_basedir
    x.opt->cfg.visit_keys({"host_os", "host_shell"});
    if (x.lang == x.Cxx)  // cxx visit config
        cxx::CxxInterpreter::test_param(x.opt->cfg, "default");
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;

    if (x.lang_out.empty())
        x.lang_out = cgn::make_path_base_working(x.opt->get_out_prefix_cfg0() + "/");
    std::string pb_basedir = api.rebase_path(x.lang_out, ".", mk);
    std::vector<std::string> proto_files;
    std::vector<std::string> pb_stems;

    mk->outputs = {pb_basedir};

    // Test and find the relevant path for the ".proto" file to 
    // any of "x.include_dir[]".
    // The out_stem is the path which .proto relavent to x.include_dir[]
    for (auto it : x.srcs) {
        if (it.size() < 6 || it.substr(it.size()-6) != ".proto")
            continue;

        proto_files += {api.locale_path(x.opt->src_prefix + it)};

        // "one of `inc`(x.include_dirs) must be an exact prefix of the `it`(x.srcs)"
        // The first matched path would represent the output relavent path
        std::string out_stem;
        for (auto _inc : x.include_dirs) {
            std::string inc2 = api.rebase_path(_inc, ".", mk);
            std::string reb = api.rebase_path(it, inc2, x.opt->src_prefix);
            if (reb[0] != '.' && reb[1] != '.') {
                out_stem = reb.substr(0, reb.size() - 6);
                break;
            }
        }

        // If no include_dir[] selected
        if (out_stem.empty()) {
            mk->errmsg = "Protoc: you must assign a include_dir "
                         "which is the prefix of src file. (" + it + ")";
            return ;
        }

        // The stem of .pb.cc file
        pb_stems += {pb_basedir + out_stem};
    }

    // NINJA[public-phony] phony-empty-file-placeholder for .pb.cc file shared in different configs
    // generate ${lang_out}/.pb.cc, ${lang_out}/_pb2.py, ... phony placeholder for specific language
    cgn::CGNTargetOpt lang_opt = *x.opt;
    lang_opt.cfg = cgn::Configuration{}; // empty_configuration
    auto pb_placeholder_target = api.create_target(&lang_opt, [&pb_stems](cgnv1::CGNTargetOpt *opt) {
        cgn::CGNTargetMaker *mid_mk = opt->confirm();
        if (mid_mk == nullptr || mid_mk->ninja == nullptr)
            return ;
        for (auto stem : pb_stems)
            for (auto suffix : {".pb.cc", ".pb.h", ".py"}) {
                auto *field = mid_mk->ninja->append_build();
                field->rule = "phony";
                field->outputs = {cgn::NinjaFile::escape_path(stem + suffix)};
            }
    });

    // Case CXX : .proto ($proto_files) -> .pb.cc ($pb_basedir, $pb_stems) -> .obj (subtarget)
    if (x.lang == x.Cxx) {
        std::string protoc_gen_stampfile = mk->out_prefix + ".protoc_stamp";
        if (mk->ninja) {
            std::string proto_argfile = mk->out_prefix + ".protorsp";
            std::ofstream fout(proto_argfile);

            // NINJA[protoc] arg file: {-I...}
            for (auto it : x.include_dirs)
                fout<<"-I" + api.rebase_path(it, ".", mk) + "\n";
            fout<<"-I" + api.get_filepath("@third_party//protobuf/repo/src") + "\n";
            
            // NINJA[protoc] arg file: {--cpp_out=... --grpc_out=...}
            fout<<"--cpp_out=" + api.shell_escape(pb_basedir, mk->trimmed_cfg["host_shell"])<<"\n";
            if (grpc_plugin_exe.size())
                fout<<"--plugin=protoc-gen-grpc=" + api.shell_escape(grpc_plugin_exe, mk->trimmed_cfg["host_shell"])<<"\n"
                    <<"--grpc_out=" + api.shell_escape(pb_basedir, mk->trimmed_cfg["host_shell"])<<"\n";
            
            // NINJA[protoc] arg file: .proto
            for (auto it : proto_files)
                fout<<it + "\n";

            mk->ninja_file_appendix += {proto_argfile};

            // NINJA[protoc] target .proto -> .pb.cc / .pb.h
            // TODO: protoc missing .dep output for included .proto files
            mk->ninja->append_include(api.get_filepath("@cgn.d//library/utility/quick_run.ninja"));
            auto *field = mk->ninja->append_build();
            field->rule = (mk->trimmed_cfg["host_os"] == "win")? "win_run_and_stamp":"unix_run_and_stamp";
            field->inputs = {mk->ninja->escape_path(protoc.outputs[0])};
            field->implicit_inputs = mk->ninja->escape_path(proto_files);
            field->variables["args"] = "@" + proto_argfile;
            field->variables["desc"] = "PROTOC " + mk->label;
            field->outputs = {mk->ninja->escape_path(protoc_gen_stampfile)};
            for (auto stem : pb_stems)
                field->implicit_inputs += {stem + ".pb.h", stem + ".pb.cc"};
        } //endif(mk->ninja)

        // ANONYMOUS_TARGET[cxx_sources] .pb.cc / .pb.h -> .obj
        cgn::CGNTargetOpt opt_cxx = *x.opt;
        opt_cxx.name = "cxx_obj";
        opt_cxx.out_parent_prefix = mk->out_prefix;
        opt_cxx.out_parent_prefix_unixsep = mk->out_prefix_unixsep;
        auto obj_target = api.create_target(&opt_cxx, 
            [&pb_stems, &pb_basedir, &protoc_gen_stampfile](cgnv1::CGNTargetOpt *opt_cxx){
                cxx::CxxSourcesContext ctx{opt_cxx};
                ctx.pub.include_dirs = ctx.include_dirs 
                    = {cgn::make_path_base_working(pb_basedir)};
                for (auto it : pb_stems)
                    ctx.srcs += {cgn::make_path_base_working(it + ".pb.cc")};
                ctx.add_dep("@third_party//protobuf", cxx::inherit);
                ctx.add_ninja_order_only_dep(protoc_gen_stampfile);
                cxx::CxxInterpreter::interpret(ctx);
            });
        if (obj_target.errmsg.size()) {
            mk->errmsg = "error on cxx_obj target: " + obj_target.errmsg;
            return ;
        }

        mk->ninja_entry = obj_target.ninja_entry;
        // merge cxx_obj target result into current one
        mk->merge_from(obj_target);
    } //endif (LANG == CXX)
}
