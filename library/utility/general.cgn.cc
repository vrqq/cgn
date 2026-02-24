#define CGN_LIBRARY_GENERAL_IMPL
#include "general.cgn.h"

// RunExecInterpreter
// ------------------

CGN_LIBRARY_GENERAL_API void RunExecInterpreter::interpret(context_type &x)
{
    std::string shell_name = x.cfg["host_shell"];
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;

    auto *rule = mk->ninja->append_rule();
    rule->name = "exec";
    for (auto &ss : x.cmd_build)
        rule->command += cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(ss, shell_name))  + " ";
    
    auto *field = mk->ninja->append_build();
    field->outputs = {mk->ninja->escape_path(mk->out_prefix + mk->NINJA_ENTRY_TARGET)};

    field->rule = "exec";
    for (auto &file : x.inputs) {
        auto fp = api.rebase_path(file, ".", mk);
        field->inputs+= {mk->ninja->escape_path(fp)};
    }
    for (auto &file : x.outputs) {
        auto fp = api.rebase_path(file, ".", mk);
        field->outputs += {mk->ninja->escape_path(fp)};
        mk->outputs += {fp};
    }
} //RunExecInterpreter::interpret


// Target Alias
// ------------
bool AliasInterpreter::AliasContext::load_named_config(const std::string &cfg_name)
{
    auto dep = api.query_config(cfg_name);
    if (dep.first.empty()) {
        load_config_errormsg = cfg_name;
        return false;
    }
    load_config_errormsg.clear();
    this->cfg = dep.first;
    return true;
}

CGN_LIBRARY_GENERAL_API void AliasInterpreter::interpret(context_type &x)
{
    if (x.load_config_errormsg.size())
        return x.opt->set_fail(x.load_config_errormsg + " config not found.");
    cgn::QuickDepContext qdep{x.opt};
    cgn::CGNTarget early = qdep.quick_dep(x.actual_label, x.cfg);
    if (early.errmsg.size())
        return x.opt->set_fail(early.errmsg);
    x.cfg.visit_keys(early.trimmed_cfg);

    // Generate CGNTarget if no cache found
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;
    mk->merge_from(early);
    mk->outputs = early.outputs;
    
    // Generate ninja file if file changed.
    if (mk->ninja) {
        auto *field = mk->ninja->append_build();
        field->rule = "phony";
        field->inputs = {mk->ninja->escape_path(early.ninja_entry)};
        field->outputs = {mk->ninja->escape_path(mk->ninja_entry)};
    }
} //AliasInterpreter::interpret


// Target Group
// ------------
CGN_LIBRARY_GENERAL_API std::vector<cgn::CGNTarget> GroupInterpreter::GroupContext::add_deps(
    std::initializer_list<std::string> labels, const cgn::Configuration &cfg
) {
    std::vector<cgn::CGNTarget> rv;
    for (auto it : labels)
        rv.push_back(quick_dep(it, cfg, true));
    return rv;
}

CGN_LIBRARY_GENERAL_API std::vector<cgn::CGNTarget> GroupInterpreter::GroupContext::add_deps(
    std::vector<std::string> labels, const cgn::Configuration &cfg
) {
    std::vector<cgn::CGNTarget> rv;
    for (auto it : labels)
        rv.push_back(quick_dep(it, cfg, true));
    return rv;
}

CGN_LIBRARY_GENERAL_API void GroupInterpreter::interpret(context_type &x)
{
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;
    mk->merge_from(x.quickdep_result);

    if (mk->ninja) {
        auto *field = mk->ninja->append_build();
        field->rule = "phony";
        field->inputs = mk->ninja->escape_path(x.quickdep_ninja_target);
        field->outputs = {mk->ninja->escape_path(mk->ninja_entry)};
    }

} //GroupInterpreter::interpret
