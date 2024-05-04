#include "../../entry/raymii_command.hpp"
#include "general.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

// Shell binary
// ------------
cgn::TargetInfos ShellBinary::interpret(context_type &x, cgn::CGNTargetOpt opt)
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


// Target Alias
// ------------
cgn::TargetInfos AliasInterpreter::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    auto real = api.analyse_target(api.absolute_label(x.actual_label, opt.src_prefix), x.cfg);
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


// Target Group
// ------------
std::vector<cgn::TargetInfos> GroupInterpreter::GroupContext::add_deps(
    std::initializer_list<std::string> labels, const cgn::Configuration &cfg
) {
    std::vector<cgn::TargetInfos> rv;
    for (auto it :labels)
        rv.push_back(add_dep(it, cfg));
    return rv;
}

cgn::TargetInfos GroupInterpreter::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    auto *field = opt.ninja->append_build();
    field->rule = "phony";
    field->inputs = x.ninja_target_dep;
    field->outputs = {opt.out_prefix + opt.BUILD_ENTRY};

    auto *def = x.merged_info.get<cgn::DefaultInfo>(true);
    def->target_label = opt.factory_ulabel;
    def->build_entry_name = opt.out_prefix + opt.src_prefix;

    return x.merged_info;
} //GroupInterpreter::interpret


// LinkAndRuntimeFiles
// -------------------
cgn::TargetInfos LinkAndRuntimeFiles::interpret(
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
