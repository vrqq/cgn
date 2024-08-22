#include "@cgn.d/library/cxx.cgn.bundle/cxx.cgn.h"
#include "proto.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

// BUGFIX:
//  since the .proto filepath may starting with '@', so we have to generate
//  a option file to `protoc @optfile`
cgn::TargetInfos ProtobufInterpreter::interpret(
    context_type &x, cgn::CGNTargetOpt opt
) {
    if (x.srcs.empty() || x.lang == x.UNDEFINED)
        throw std::runtime_error{opt.factory_ulabel + "empty src or lang"};
    auto host_cfg = api.query_config("host_release");
    
    // return via CxxInterpreter
    cxx::CxxSourcesContext cxx_ctx(x.cfg, opt);

    // load protoc exe
    cgn::CGNTarget protoc = api.analyse_target(x.protoc, *host_cfg.first);
    api.add_adep_edge(host_cfg.second, opt.adep);
    auto *pbcout = protoc.infos.get<cgn::DefaultInfo>();
    if (!pbcout || pbcout->outputs.empty())
        throw std::runtime_error{opt.factory_ulabel +" protoc not found: " + x.protoc};
    api.add_adep_edge(protoc.adep, opt.adep);
    std::string pbcexe = pbcout->outputs[0];

    // load grpc plugin
    std::string grpc_plugin_exe;
    if (x.grpc_plugin_label.size()) {
        auto rv = cxx_ctx.add_dep(x.grpc_plugin_label, *host_cfg.first, cxx::private_dep);
        grpc_plugin_exe = rv.get<cgn::DefaultInfo>()->outputs[0];
    }

    // make protoc @opt
    std::string proto_argfile = opt.out_prefix + ".protorsp";
    std::ofstream fout(proto_argfile);
    
    // args at {protoc '--cpp_out=...'}
    if (x.lang_src_outdir.size())
        x.lang_src_outdir = api.locale_path(opt.src_prefix + x.lang_src_outdir + "/");
    else
        x.lang_src_outdir = opt.out_prefix + "langsrc/";
    cxx_ctx.include_dirs = cxx_ctx.pub.include_dirs 
                     = {api.rebase_path(x.lang_src_outdir, opt.src_prefix)};
    cxx_ctx.add_dep("@third_party//protobuf:libprotobuf", cxx::inherit);

    // args at {protoc '-I...'}
    // TBD: read protoc include_dir from CxxInfo target return?
    for (auto it : x.include_dirs)
        fout<<"-I" + api.locale_path(opt.src_prefix + it) + "\n";
    fout<<"-I" + api.get_filepath("@third_party//protobuf/repo/src") + "\n";

    // arg for --cpp_out, --grpc_out
    fout<<"--cpp_out=" + two_escape(x.lang_src_outdir)<<"\n";
    if (grpc_plugin_exe.size())
        fout<<"--plugin=protoc-gen-grpc=" + two_escape(grpc_plugin_exe)<<"\n"
            <<"--grpc_out=" + two_escape(x.lang_src_outdir)<<"\n";

    // std::string ninja_args;
    // for (auto it : x.include_dirs)
    //     ninja_args += "-I" + two_escape(opt.src_prefix + it) + " ";
    // ninja_args += "-I" 
    //     + two_escape(api.get_filepath("@third_party//protobuf/repo/src"));

    // arg at {protoc '.proto'}
    std::vector<std::string> ninja_inputfiles = {proto_argfile};

    std::vector<std::string> lang_pbout_njesc;
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
        if (out_stem.empty())
            throw std::runtime_error{"Protoc: you must assign a include_dir"
                "which is the prefix of src file. " + opt.factory_ulabel};

        std::string protofile = api.locale_path(opt.src_prefix + it);
        fout<<protofile<<"\n";
        ninja_inputfiles += {protofile};
        
        if (x.lang == x.Cxx) {
            std::string s1 = x.lang_src_outdir + out_stem;
            lang_pbout_njesc += {opt.ninja->escape_path(s1 + ".pb.cc"), 
                                 opt.ninja->escape_path(s1 + ".pb.h")};
            cxx_ctx.srcs += {api.rebase_path(s1 + ".pb.cc", opt.src_prefix)};
            if (grpc_plugin_exe.size()) {
                lang_pbout_njesc += {opt.ninja->escape_path(s1 + ".grpc.pb.cc"),
                                     opt.ninja->escape_path(s1 + ".grpc.pb.h")};
                cxx_ctx.srcs += {
                    api.rebase_path(s1 + ".grpc.pb.cc", opt.src_prefix)
                };
            }
        }
    }

    opt.ninja->append_include(
        api.get_filepath("@cgn.d//library/general.cgn.bundle/rule.ninja")
    );

    // *.proto => *.pb.h / *.pb.cc
    auto *field = opt.ninja->append_build();
    field->rule = "run";
    field->inputs  = {opt.ninja->escape_path(pbcexe)};
    field->implicit_inputs = ninja_inputfiles;
    field->variables["args"] = "@" + proto_argfile;
    field->variables["desc"] = "PROTOC " + opt.factory_ulabel;
    field->outputs = lang_pbout_njesc;


    auto rv = cxx::CxxInterpreter::interpret(cxx_ctx, opt);
    rv.get<cgn::DefaultInfo>()->enforce_keep_order = true;
    
    return rv;
}
