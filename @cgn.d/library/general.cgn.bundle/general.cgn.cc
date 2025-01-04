#define GENERAL_CGN_BUNDLE_IMPL
#include "../../v1/raymii_command.hpp"
#include "general.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

// Shell binary
// ------------
// TODO: generate a shell script
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
            field->outputs += {opt->ninja->escape_path(file)};
        }

        auto *entry    = opt->ninja->append_build();
        entry->rule    = "phony";
        entry->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
        entry->inputs  = field->outputs;
    }
    else
        field->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};

    for (auto &fname : x.outputs)
        field->implicit_outputs += {opt->ninja->escape_path(
                opt->out_prefix + cgn::Tools::locale_path(fname))};

    opt->result.outputs = x.outputs;
} //ShellBinary::interpret()


// Target Alias
// ------------
bool AliasInterpreter::AliasContext::load_named_config(const std::string &cfg_name)
{
    auto dep = api.query_config(cfg_name);
    if (dep.second == nullptr) {
        load_config_errormsg = cfg_name;
        return false;
    }
    load_config_errormsg.clear();
    this->cfg = dep.first;
    opt->quickdep_early_anodes.push_back(dep.second);
    return true;
}

GENERAL_CGN_BUNDLE_API void AliasInterpreter::interpret(context_type &x)
{
    if (x.load_config_errormsg.size()) {
        x.opt->confirm_with_error(x.load_config_errormsg + " config not found.");
        return ;
    }
    cgn::CGNTarget early = x.opt->quick_dep(
            api.absolute_label(x.actual_label, x.opt->factory_label), x.cfg);
    if (early.errmsg.size()) {
        x.opt->confirm_with_error(early.errmsg);
        return ;
    }
    x.cfg.visit_keys(early.trimmed_cfg);

    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;
    opt->result.ninja_dep_level = early.ninja_dep_level;
    opt->result.merge_from(early);
    opt->result.outputs = early.outputs;
    
    auto *field = opt->ninja->append_build();
    field->rule = "phony";
    field->inputs = {opt->ninja->escape_path(early.ninja_entry)};
    field->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
} //AliasInterpreter::interpret


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
        }
        rv.push_back(std::move(tgt));
    }
    return rv;
}

GENERAL_CGN_BUNDLE_API void GroupInterpreter::interpret(context_type &x)
{
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    auto *field = opt->ninja->append_build();
    field->rule = "phony";
    field->inputs = opt->ninja->escape_path(x.deps_ninja_entry);
    field->implicit_inputs = opt->ninja->escape_path(opt->quickdep_ninja_full);
    field->order_only      = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
    field->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
} //GroupInterpreter::interpret
