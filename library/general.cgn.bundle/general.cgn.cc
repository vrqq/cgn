#define GENERAL_CGN_BUNDLE_IMPL
#include "../../entry/raymii_command.hpp"
#include "../../std_operator.hpp"
#include "general.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

// Shell binary
// ------------
GENERAL_CGN_BUNDLE_API cgn::TargetInfos ShellBinary::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    // if user cmd_analysis existed
    if (x.cmd_analysis.size()) {
        std::string cmd;
        for (auto &it : x.cmd_analysis)
            cmd += api.shell_escape(it) + " ";
        auto exerv = raymii::Command::exec(cmd);
        if (exerv.exitstatus != 0)
            throw std::runtime_error{opt.factory_ulabel 
                + " Custom command exec failed:\n" + exerv.output};
    }

    //.build_stamp
    // static std::string shell_rule_path = cgn.get_filepath(shell_rule);
    // opt.ninja->append_include(shell_rule_path);
    auto *rule = opt.ninja->append_rule();
    rule->name = "exec";
    for (auto &ss : x.cmd_build)
        rule->command += two_escape(ss) + " ";

    auto *field = opt.ninja->append_build();
    field->rule = "exec";
    field->implicit_inputs = {x.inputs.begin(), x.inputs.end()};
    if (x.outputs.size()) {
        for (auto &file : x.outputs) {
            file = api.locale_path(opt.src_prefix + file);
            field->outputs.push_back(opt.ninja->escape_path(file));
        }

        auto *entry    = opt.ninja->append_build();
        entry->rule    = "phony";
        entry->outputs = {opt.out_prefix + opt.BUILD_ENTRY};
        entry->inputs  = field->outputs;
    }
    else
        field->outputs = {opt.out_prefix + opt.BUILD_ENTRY};

    for (auto &fname : x.outputs)
        field->implicit_outputs.push_back(opt.out_prefix 
                        + cgn::Tools::locale_path(fname));

    cgn::TargetInfos rv;
    cgn::DefaultInfo *oinf = rv.get<cgn::DefaultInfo>(true);
    oinf->outputs.insert(oinf->outputs.end(), x.outputs.begin(), x.outputs.end());
    return rv;
} //ShellBinary::interpret()

// Copy
// ----
GENERAL_CGN_BUNDLE_API cgn::TargetInfos CopyInterpreter::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    if (x.from.size() != x.to.size())
        throw std::runtime_error{opt.factory_ulabel + " from[] and to[] are not same size."};

    // Since ninja cannot recognize folder in INPUT, we have to let target
    // re-analyse every time. For example someone was originally a regular 
    // file, but now it has become a folder.

    std::string rulepath = api.get_filepath("@cgn.d//library/general.cgn.bundle/rule.ninja");
    opt.ninja->append_include(rulepath);

    // using 'cp' in unix-like os (rule.ninja)
    std::string rule = (api.get_host_info().os != "win"? "unix_cp" : "win_cp");

    std::vector<std::string> collection;
    for (std::size_t i=0; i<x.from.size(); i++) {
        auto *build = opt.ninja->append_build();
        build->rule    = rule;
        build->inputs  = {opt.ninja->escape_path(x.from[i])};
        build->outputs = {opt.ninja->escape_path(x.to[i])};
        collection += build->outputs;
    }

    // ninja phony entry: collection + deps
    auto *phony = opt.ninja->append_build();
    phony->rule = "PHONY";
    phony->inputs = collection + x.ninja_target_dep;

    // Generate return value
    cgn::TargetInfos &rv = x.merged_info;
    auto *def = rv.get<cgn::DefaultInfo>(true);
    def->outputs += x.to;

    return rv;
} //CopyInterpreter::interpret


// Target Alias
// ------------
GENERAL_CGN_BUNDLE_API cgn::TargetInfos AliasInterpreter::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    auto real = api.analyse_target(api.absolute_label(x.actual_label, opt.factory_ulabel), x.cfg);
    auto *definfo = real.infos.get<cgn::DefaultInfo>(true);
    api.add_adep_edge(real.adep, opt.adep);

    auto *field = opt.ninja->append_build();
    field->rule = "phony";
    field->inputs = {definfo->build_entry_name};
    field->outputs = {opt.out_prefix + opt.BUILD_ENTRY};

    definfo->target_label = opt.factory_ulabel;
    definfo->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;
    return real.infos;
} //AliasInterpreter::interpret


GENERAL_CGN_BUNDLE_API cgn::CGNTarget DynamicAliasInterpreter::DynamicAliasContext::load_target(
    const std::string &label, const cgn::Configuration &cfg
) {
    auto rv = api.analyse_target(label, cfg);
    actual_target_infos = rv.infos;
    if (!actual_target_infos.empty())
        self_def = *actual_target_infos.get<cgn::DefaultInfo>();
    return rv;
} //DynamicAliasContext::load_target

GENERAL_CGN_BUNDLE_API cgn::TargetInfos DynamicAliasInterpreter::interpret(
    context_type &x, cgn::CGNTargetOpt opt
) {
    if (x.actual_target_infos.empty())
        throw std::runtime_error{ opt.factory_name + " no valid target loaded." };

    // DefaultInfo cannot be modified.
    auto *def = x.actual_target_infos.get<cgn::DefaultInfo>();
    *def = x.self_def;
    def->target_label = opt.factory_ulabel;
    return x.actual_target_infos;
} //DynamicAliasContext::interpret


// Target Group
// ------------
GENERAL_CGN_BUNDLE_API std::vector<cgn::TargetInfos> GroupInterpreter::GroupContext::add_deps(
    std::initializer_list<std::string> labels, const cgn::Configuration &cfg
) {
    std::vector<cgn::TargetInfos> rv;
    for (auto it :labels)
        rv.push_back(add_dep(it, cfg));
    return rv;
}

GENERAL_CGN_BUNDLE_API cgn::TargetInfos GroupInterpreter::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    auto *field = opt.ninja->append_build();
    field->rule = "phony";
    field->inputs = x.ninja_target_dep;
    field->outputs = {opt.out_prefix + opt.BUILD_ENTRY};

    auto *def = x.merged_info.get<cgn::DefaultInfo>(true);
    def->target_label = opt.factory_ulabel;
    def->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;

    return x.merged_info;
} //GroupInterpreter::interpret


// LinkAndRuntimeFiles
// -------------------
GENERAL_CGN_BUNDLE_API cgn::TargetInfos LinkAndRuntimeFiles::interpret(
    context_type &x, cgn::CGNTargetOpt opt
) {
    cgn::TargetInfos rv;
    rv.set((const cgn::LinkAndRunInfo &)x);

    auto *def = rv.get<cgn::DefaultInfo>(true);
    def->target_label = opt.factory_ulabel;
    def->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;

    auto *field = opt.ninja->append_build();
    field->rule = "phony";
    field->outputs = {opt.out_prefix + opt.BUILD_ENTRY};

    return rv;
}
