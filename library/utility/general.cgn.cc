#define CGN_LIBRARY_GENERAL_IMPL
#include "general.cgn.h"

// RunExecInterperter
// ------------------

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}
CGN_LIBRARY_GENERAL_API void RunExecInterperter::interpret(context_type &x)
{
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    auto *rule = opt->ninja->append_rule();
    rule->name = "exec";
    for (auto &ss : x.cmd_build)
        rule->command += two_escape(ss) + " ";
    
    auto *field = opt->ninja->append_build();
    field->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};

    field->rule = "exec";
    for (auto &file : x.inputs) {
        auto fp = api.rebase_path(file, ".", opt);
        field->inputs+= {opt->ninja->escape_path(fp)};
    }
    for (auto &file : x.outputs) {
        auto fp = api.rebase_path(file, ".", opt);
        field->outputs += {opt->ninja->escape_path(fp)};
        opt->result.outputs += {fp};
    }
} //RunExecInterperter::interpret


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

CGN_LIBRARY_GENERAL_API void AliasInterpreter::interpret(context_type &x)
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
    opt->result.outputs = early.outputs;
    
    auto *field = opt->ninja->append_build();
    field->rule = "phony";
    field->inputs = {opt->ninja->escape_path(early.ninja_entry)};
    field->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};
} //AliasInterpreter::interpret


// Target Group
// ------------
CGN_LIBRARY_GENERAL_API std::vector<cgn::CGNTarget> GroupInterpreter::GroupContext::add_deps(
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

CGN_LIBRARY_GENERAL_API void GroupInterpreter::interpret(context_type &x)
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
