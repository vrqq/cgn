#include "shell_script.cgn.h"

// opt->cfg["host_shell"] would be visited
void ShellScriptWorker::preconfig(cgn::CGNTargetOpt *opt) {
    this->opt = opt;
    opt->cfg.visit_keys({"host_shell"});
    if (opt->cfg["host_os"] != "win")
        _content += "#!/bin/sh\n";
}

void ShellScriptWorker::append_setenv(const std::string &key, const std::string &value) {
    return append_setenv({{key, value}});
}

void ShellScriptWorker::append_setenv(const std::unordered_map<std::string, std::string> &data) {
    for (auto &it : data) {
        _content += (opt->cfg["host_os"] == "win"? "SET ": "export ")
             + api.shell_escape(it.first, opt->cfg["host_shell"]) + "=" + it.second + "\n";
    }
}

void ShellScriptWorker::append_pushd(const cgn::CGNPath &path, cgn::CGNTargetMaker *mk) {
    if (mk == nullptr && path.type == path.BASE_ON_OUTPUT)
        throw std::runtime_error{"ShellScriptWorker.append_pushd() : cannot rebase path " + path.to_string()};
    std::string p = mk?api.rebase_path(path, "", mk):api.rebase_path(path, "", opt);
    _content += "pushd " + api.shell_escape(p, opt->cfg["host_shell"]) + "\n";
}

void ShellScriptWorker::append_popd() {
    _content += "popd\n";
}

void ShellScriptWorker::append_cmd(const std::vector<std::string> &args) {
    std::string line;
    for (auto it : args)
        line += api.shell_escape(it, opt->cfg["host_shell"]) + " ";
    append_escaped_cmd(line);
}

void ShellScriptWorker::append_escaped_cmd(const std::string &line) {
    _content += "\n";
    if (opt->cfg["host_shell"] == "cmd") {
        _content += line + "\n";
        _content += "if %ERRORLEVEL% NEQ 0 (exit /b %ERRORLEVEL%)\n";
    }
    else {
        _content += line + "\n"
                  + "RET=$?\n";
        _content += "if [ $RET -ne 0 ]; then\n"
                    "  exit $RET\n"
                    "fi\n";
    }
}

void ShellScriptWorker::append_stamp(const cgn::CGNPath &stampfile) {
    std::string line = opt->cfg["host_os"] == "win"? "type nul > ": "touch ";
    line += api.shell_escape(api.rebase_path(stampfile, ".", opt), opt->cfg["host_shell"]);
    append_escaped_cmd(line);
}

// Get shell script content
std::string ShellScriptWorker::to_string() const {
    return _content;
}

//@return : true for successful or file content unchanged, false for error occured.
bool ShellScriptWorker::write_to_file(const std::string &filepath) {
    api.write_file_content_if_changed(filepath, _content);
    return true;
}


//
// ShellScript Interpreter
// -----------------------

void ShellScript::interpret(context_type &x)
{
    x.opt->cfg.visit_keys({"host_os"});
    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
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

    for (auto p : x.analysis_outputs)
        mk->outputs += {api.rebase_path(p, ".", mk)};

    if (!mk->ninja)
        return ;
    
    // write script file
    std::string bat_filename = mk->out_prefix 
                        + (x.cfg["host_os"] == "win"? ".bat": ".sh");
    x.worker.write_to_file(bat_filename);
    mk->ninja_file_appendix += {bat_filename};

    //write build.ninja
    std::string rulepath = api.get_filepath("@cgn.d//library/utility/runbat.ninja");
    mk->ninja->append_include(rulepath);

    auto *field = mk->ninja->append_build();
    field->rule = rule_name;
    field->variables["factory_name"] = mk->label;
    field->variables["restat"] = "1";
    field->inputs  = {mk->ninja->escape_path(bat_filename)};
    for (auto &p : x.extra_watch_files)
        field->implicit_inputs += {mk->ninja->escape_path(api.rebase_path(p, ".", mk))};
    field->order_only = x.quickdep_ninja_target;

    // write default .stamp if no output specified.
    if (x.script_outputs.empty()){
        x.worker.append_stamp(cgn::CGNPath(cgn::CGNPath::BASE_ON_OUTPUT, mk->NINJA_ENTRY_TARGET));
        x.script_outputs += {cgn::CGNPath(cgn::CGNPath::BASE_ON_OUTPUT, mk->NINJA_ENTRY_TARGET)};
        field->outputs += {mk->ninja->escape_path(mk->out_prefix + mk->NINJA_ENTRY_TARGET)};
    }
    else {
        for (auto &p : x.script_outputs) {
            std::string op = api.rebase_path(p, ".", mk);
            field->outputs += {mk->ninja->escape_path(op)};
        }
        auto *phony = mk->ninja->append_build();
        phony->rule = "phony";
        phony->inputs = field->outputs;
        phony->outputs = {mk->ninja->escape_path(mk->ninja_entry)};
    }

} //ShellScript::interpret()
