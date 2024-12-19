#define GENERAL_CGN_BUNDLE_IMPL
#include "../../v1/raymii_command.hpp"
#include "general.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

// Shell binary
// ------------
GENERAL_CGN_BUNDLE_API void ShellBinary::interpret(context_type &x)
{
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    // if user cmd_analysis existed
    if (x.cmd_analysis.size()) {
        std::string cmd;
        for (auto &it : x.cmd_analysis)
            cmd += api.shell_escape(it) + " ";
        auto exerv = raymii::Command::exec(cmd);
        if (exerv.exitstatus != 0){
            opt->result.errmsg = " Custom command exec failed:\n" + exerv.output;
            return ;
        }
    }

    //.build_stamp
    // static std::string shell_rule_path = cgn.get_filepath(shell_rule);
    // opt.ninja->append_include(shell_rule_path);
    auto *rule = opt->ninja->append_rule();
    rule->name = "exec";
    for (auto &ss : x.cmd_build)
        rule->command += two_escape(ss) + " ";

    auto *field = opt->ninja->append_build();
    field->rule = "exec";
    field->implicit_inputs = {x.inputs.begin(), x.inputs.end()};
    if (x.outputs.size()) {
        for (auto &file : x.outputs) {
            file = api.locale_path(opt->src_prefix + file);
            field->outputs.push_back(opt->ninja->escape_path(file));
        }

        auto *entry    = opt->ninja->append_build();
        entry->rule    = "phony";
        entry->outputs = {opt->out_prefix + opt->BUILD_ENTRY};
        entry->inputs  = field->outputs;
    }
    else
        field->outputs = {opt->out_prefix + opt->BUILD_ENTRY};

    for (auto &fname : x.outputs)
        field->implicit_outputs.push_back(opt->out_prefix 
                        + cgn::Tools::locale_path(fname));

    opt->result.outputs = x.outputs;
} //ShellBinary::interpret()

// Copy
// ----
GENERAL_CGN_BUNDLE_API void CopyInterpreter::interpret(context_type &x)
{
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    if (x.from.size() != x.to.size()) {
        opt->result.errmsg = "from[] and to[] are not same size.";
        return ;
    }

    // Since ninja cannot recognize folder in INPUT, we have to let target
    // re-analyse every time. For example someone was originally a regular 
    // file, but now it has become a folder.

    std::string rulepath = api.get_filepath("@cgn.d//library/general.cgn.bundle/rule.ninja");
    if (!opt->ninja->is_file_included(rulepath))
        opt->ninja->append_include(rulepath);

    // using 'cp' in unix-like os (rule.ninja)
    std::string rule = (api.get_host_info().os != "win"? "unix_cp" : "win_cp");

    std::vector<std::string> collection;
    for (std::size_t i=0; i<x.from.size(); i++) {
        x.from[i] = api.locale_path(opt->src_prefix + x.from[i]);
        x.to[i] = api.locale_path(opt->src_prefix + x.to[i]);
        auto *build = opt->ninja->append_build();
        build->rule    = rule;
        build->inputs  = {opt->ninja->escape_path(x.from[i])};
        build->outputs = {opt->ninja->escape_path(x.to[i])};
        build->implicit_inputs = opt->quickdep_ninja_full;
        build->order_only      = opt->quickdep_ninja_dynhdr;
        collection += build->outputs;
    }

    // ninja phony entry: collection + deps
    auto *phony = opt->ninja->append_build();
    phony->rule = "PHONY";
    phony->inputs = collection;

    // Generate return value
    cgn::TargetInfos &rv = x.merged_info;
    auto *def = rv.get<cgn::DefaultInfo>(true);
    def->outputs += x.to;

    return rv;
} //CopyInterpreter::interpret


// Target Alias
// ------------
void AliasInterpreter::AliasContext::load_named_config(const std::string &cfg_name)
{
    auto dep = api.query_config(cfg_name);
    if (dep.first == nullptr) {
        opt->confirm_with_error(cfg_name + " not found");
        return ;
    }
    this->cfg = *dep.first;
    opt->quickdep_early_anodes.push_back(dep.second);
}

GENERAL_CGN_BUNDLE_API void AliasInterpreter::interpret(context_type &x)
{
    cgn::CGNTarget early = x.opt->quick_dep(
            api.absolute_label(x.actual_label, x.opt->factory_label), x.cfg);
    if (early.errmsg.size()) {
        x.opt->confirm_with_error(early.errmsg);
        return ;
    }
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;
    opt->result.merge_from(early);
    opt->result.outputs = early.outputs;
    
    auto *field = opt->ninja->append_build();
    field->rule = "phony";
    field->inputs = {early.ninja_entry};
    field->outputs = {opt->out_prefix + opt->BUILD_ENTRY};
} //AliasInterpreter::interpret


GENERAL_CGN_BUNDLE_API cgn::CGNTarget DynamicAliasInterpreter::DynamicAliasContext::load_target(
    const std::string &label, const cgn::Configuration &cfg
) {
    actual_target = opt->quick_dep(label, cfg);
    return actual_target;
} //DynamicAliasContext::load_target

GENERAL_CGN_BUNDLE_API void DynamicAliasInterpreter::interpret(context_type &x)
{
    if (x.actual_target.errmsg.size()) {
        x.opt->confirm_with_error(x.opt->factory_name + " no valid target loaded." );
        return ;
    }
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    opt->result.merge_from(x.actual_target);
    opt->result.outputs = x.actual_target.outputs;
    
    auto *field = opt->ninja->append_build();
    field->rule = "phony";
    field->inputs = {x.actual_target.ninja_entry};
    field->outputs = {opt->out_prefix + opt->BUILD_ENTRY};
} //DynamicAliasContext::interpret


// Target Group
// ------------
GENERAL_CGN_BUNDLE_API std::vector<cgn::CGNTarget> GroupInterpreter::GroupContext::add_deps(
    std::initializer_list<std::string> labels, const cgn::Configuration &cfg
) {
    std::vector<cgn::CGNTarget> rv;
    for (auto it :labels){
        auto tgt = opt->quick_dep(it, cfg);
        if (tgt.errmsg.empty()) {
            deps_ninja_entry.push_back(tgt.ninja_entry);
            rv.push_back(std::move(tgt));
        }
    }
    return rv;
}

GENERAL_CGN_BUNDLE_API void GroupInterpreter::interpret(context_type &x)
{
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    auto *field = opt->ninja->append_build();
    field->rule = "phony";
    field->inputs = x.deps_ninja_entry;
    field->outputs = {opt->out_prefix + opt->BUILD_ENTRY};
    for (auto )

    auto *def = x.merged_info.get<cgn::DefaultInfo>(true);
    def->target_label = opt.factory_ulabel;
    def->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;

    return x.merged_info;
} //GroupInterpreter::interpret

/* TODO
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
*/