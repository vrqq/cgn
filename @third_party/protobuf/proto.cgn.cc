#include "cgn.d/library/cxx.cgn.bundle/cxx.cgn.h"
#include "proto.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

cgn::TargetInfos ProtobufInterpreter::interpret(
    context_type &x, cgn::CGNTargetOpt opt
) {
    if (x.srcs.empty() || x.lang == x.UNDEFINED)
        throw std::runtime_error{opt.factory_ulabel + "empty src or lang"};
    cgn::CGNTarget protoc = api.analyse_target(x.protoc, 
                        *api.query_config("host_release"));
    auto *pbcout = protoc.infos.get<cgn::DefaultInfo>();
    if (!pbcout || pbcout->outputs.empty())
        throw std::runtime_error{opt.factory_ulabel +" protoc not found: " + x.protoc};
    
    // args at {'protoc' }
    std::string pbcexe = pbcout->outputs[0];

    // args at {protoc '--cpp_out=...'}
    if (x.lang_src_outdir.size())
        x.lang_src_outdir = api.locale_path(opt.src_prefix + x.lang_src_outdir + "/");
    else
        x.lang_src_outdir = opt.out_prefix + "langsrc/";
    
    // args at {protoc '-I...'}
    std::string pbcflags_2esc;
    for (auto it : x.include_dirs)
        pbcflags_2esc += "-I" + two_escape(opt.src_prefix + it) + " ";
    pbcflags_2esc += "-I" 
        + two_escape(api.get_filepath("@third_party//protobuf/repo/src"));
        // + " -I" + two_escape(api.rebase_path(opt.src_prefix, "."));

    // arg at {protoc '.proto'}
    std::vector<std::string> proto_fullpath_njesc;

    std::vector<std::string> lang_pbout_njesc;
    std::vector<std::string> src_for_cxxtarget;
    for (auto it : x.srcs) {
        if (it.size() < 6 || it.substr(it.size()-6) != ".proto")
            continue;
        proto_fullpath_njesc.push_back(
            opt.ninja->escape_path(api.locale_path(opt.src_prefix + it))
        );
        std::string stem = it.substr(0, it.size() - 6);
        
        if (x.lang == x.Cxx) {
            std::string s1 = x.lang_src_outdir + stem;
            lang_pbout_njesc += {opt.ninja->escape_path(s1 + ".pb.cc"), 
                                 opt.ninja->escape_path(s1 + ".pb.h")};
            src_for_cxxtarget += {api.rebase_path(s1 + ".pb.cc", opt.src_prefix)};
        }
    }

    opt.ninja->append_include(
        api.get_filepath("@cgn.d//library/general.cgn.bundle/rule.ninja")
    );

    auto *field = opt.ninja->append_build();
    field->rule = "run";
    field->inputs  = {opt.ninja->escape_path(pbcexe)};
    field->inputs += proto_fullpath_njesc;
    field->variables["args"] = pbcflags_2esc
        + " --cpp_out=" + two_escape(x.lang_src_outdir);
    field->variables["desc"] = "PROTOC " + opt.factory_ulabel;
    field->outputs = lang_pbout_njesc;

    cxx::CxxSourcesContext ctx(x.cfg, opt);
    ctx.srcs = src_for_cxxtarget;
    ctx.include_dirs = ctx.pub.include_dirs 
                     = {api.rebase_path(x.lang_src_outdir, opt.src_prefix)};
    ctx.add_dep("@third_party//protobuf:libprotobuf", cxx::inherit);
    auto rv = cxx::CxxInterpreter::interpret(ctx, opt);

    rv.get<cgn::DefaultInfo>()->enforce_keep_order = true;
    
    return rv;
}
