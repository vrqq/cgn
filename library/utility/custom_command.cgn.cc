#define CGN_UTILITY_IMPL
#include <fstream>
#include "custom_command.cgn.h"

static std::string expand_cgnpath(const cgn::CGNPath &it, cgn::CGNTargetOpt *opt)
{
    if (it.type == it.BASE_ON_OUTPUT)
        return api.rebase_path(it.rpath, ".", opt->out_prefix);
    else if (it.type == it.BASE_ON_SCRIPT_SRC)
        return api.rebase_path(it.rpath, ".", opt->src_prefix);
    return it.rpath;
}

void CustomCommand::append_setenv(const std::string &key, const std::string &value)
{
    return append_setenv({{key, value}});
}

void CustomCommand::append_setenv(const std::unordered_map<std::string, std::string> &data)
{
    std::string line;
    for (auto &it : data) {
        line += (cfg["os"] == "win"? "SET ": "export")
             + api.shell_escape(it.first) + "=" + it.second + "\n";
    }
    script_content.push_back({line, {}});
}

void CustomCommand::append_pushd(cgn::CGNPath &path)
{
    script_content.push_back({"", [path](cgn::CGNTargetOpt *opt){
        std::string dir = expand_cgnpath(path, opt);
        return "pushd " + api.rebase_path(dir, "");
    }});
}

void CustomCommand::append_cmd(const std::vector<std::string> &args)
{
    std::string line;
    for (auto it : args)
        line += api.shell_escape(it) + " ";
    script_content.push_back({line, {}});
    if (cfg["os"] == "win")
        script_content.push_back({
            "if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)\n"
        ,{}});
    else
        script_content.push_back({
            "if [ $? -ne 0 ]; then\n"
            "  exit $?\n"
            "fi\n"
        ,{}});
}

void CustomInterpreter::interpret(context_type &x)
{
    std::string stamp_cmd_prefix;
    std::string rule_name;
    if (x.cfg["os"] == "win") {
        stamp_cmd_prefix = "type nul > ";
        rule_name = "run_bat_cmd";
    }
    else {
        stamp_cmd_prefix = "touch ";
        rule_name = "run_bat_bash";
    }
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    if (x.phase2_fn)
        x.phase2_fn(x, opt);

    if (opt->file_unchanged)
        return ;
    std::string phony_file = opt->out_prefix + opt->BUILD_ENTRY;
    if (x.script_content.size()) {
        std::string bat_file = opt->out_prefix + ".bat";

        std::ofstream fbat(bat_file);
        if (x.cfg["os"] == "win")
            fbat<<"@echo off\n";
        for (auto ln : x.script_content)
            if (ln.first.size())
                fbat<<ln.first<<"\n";
            else
                fbat<<ln.second(opt)<<"\n";
        fbat<<stamp_cmd_prefix + phony_file + "\n";

        std::string rulepath = api.get_filepath("@cgn.d//library/utility/runbat.ninja");
        opt->ninja->append_include(rulepath);

        auto *field = opt->ninja->append_build();
        field->rule = rule_name;
        field->variables["factory_name"] = opt->factory_label;
        field->inputs  = {opt->ninja->escape_path(bat_file)};
        field->outputs = {opt->ninja->escape_path(phony_file)};
        field->implicit_inputs = opt->ninja->escape_path(opt->quickdep_ninja_full);
        field->order_only      = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
        for (auto it : x.cmd_inputs)
            field->implicit_inputs += {
                opt->ninja->escape_path(expand_cgnpath(it, opt))
            };
        for (auto it : x.cmd_outputs)
            field->outputs += {
                opt->ninja->escape_path(expand_cgnpath(it, opt))
            };
    }
    else {
        auto *phony = opt->ninja->append_build();
        phony->rule = "phony";
        phony->implicit_inputs = opt->ninja->escape_path(opt->quickdep_ninja_full);
        phony->order_only      = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
        phony->outputs = {opt->ninja->escape_path(phony_file)};
    }
}
