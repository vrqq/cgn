#define CGN_UTILITY_IMPL
#include <fstream>
#include <sstream>
#include "custom_command.cgn.h"

void CustomCommand::append_setenv(const std::string &key, const std::string &value)
{
    return append_setenv({{key, value}});
}

void CustomCommand::append_setenv(const std::unordered_map<std::string, std::string> &data)
{
    std::string line;
    for (auto &it : data) {
        line += (cfg["os"] == "win"? "SET ": "export ")
             + api.shell_escape(it.first) + "=" + it.second + "\n";
    }
    script_content.push_back({line, {}});
}

void CustomCommand::append_pushd(const cgn::CGNPath &path)
{
    script_content.push_back({"", [path, this](cgn::CGNTargetOpt *opt) {
        return "pushd " + rebase_path(path, "");
    }});
}

void CustomCommand::append_popd()
{
    script_content.push_back({"", [](cgn::CGNTargetOpt *){ return "popd"; }});
}

void CustomCommand::append_cmd(const std::vector<std::string> &args)
{
    std::string line;
    for (auto it : args)
        line += api.shell_escape(it) + " ";
    append_escaped_cmd(line);
}

void CustomCommand::append_escaped_cmd(const std::string &line)
{
    script_content.push_back({line, {}});
    if (cfg["host_os"] == "win")
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
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    std::string stamp_cmd_prefix;
    std::string rule_name;
    if (x.cfg["host_os"] == "win") {
        stamp_cmd_prefix = "type nul > ";
        rule_name = "run_bat_cmd";
    }
    else {
        stamp_cmd_prefix = "touch ";
        rule_name = "run_bat_bash";
    }

    opt->result.ninja_dep_level = cgn::CGNTarget::NINJA_LEVEL_DYNDEP;

    for (auto p : x.analysis_outputs)
        opt->result.outputs += {x.rebase_path(p)};

    if (opt->file_unchanged)
        return ;
    std::string phony_file = opt->out_prefix + opt->BUILD_ENTRY;
    if (x.script_content.size()) {
        std::string bat_file = opt->out_prefix 
                             + (x.cfg["host_os"] == "win"? ".bat": ".sh");

        std::string bat_content;
        if (x.cfg["host_os"] == "win")
            bat_content += "@echo off\n";
        for (auto ln : x.script_content)
            if (ln.first.size())
                bat_content += ln.first + "\n";
            else
                bat_content += ln.second(opt) + "\n";
        bat_content += stamp_cmd_prefix + phony_file + "\n";

        // read existing bat_file, update if content modified.
        bool batfile_need_update = true;
        {
            std::ifstream fin(bat_file);
            if (fin) {
                std::stringstream bat_last;
                bat_last<<fin.rdbuf();
                if (bat_last.str() == bat_content)
                    batfile_need_update = false;
            }
        }
        if (batfile_need_update) {
            std::ofstream fbat(bat_file);
            fbat<<bat_content;
        }

        std::string rulepath = api.get_filepath("@cgn.d//library/utility/runbat.ninja");
        opt->ninja->append_include(rulepath);

        auto *field = opt->ninja->append_build();
        field->rule = rule_name;
        field->variables["factory_name"] = opt->factory_label;
        field->inputs  = {opt->ninja->escape_path(bat_file)};
        field->outputs = {opt->ninja->escape_path(phony_file)};
        field->implicit_inputs = opt->ninja->escape_path(opt->quickdep_ninja_full);
        field->order_only      = opt->ninja->escape_path(opt->quickdep_ninja_dynhdr);
        for (auto it : x.watch_inputs)
            field->implicit_inputs += {
                opt->ninja->escape_path(x.rebase_path(it))
            };
        for (auto it : x.watch_outputs)
            field->outputs += {
                opt->ninja->escape_path(x.rebase_path(it))
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
