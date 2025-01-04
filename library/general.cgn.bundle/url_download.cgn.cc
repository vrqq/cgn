#include "url_download.cgn.h"

void URLDownloader::interpret(URLDownloader::context_type &x)
{
    if (x.outputs.size() != 1) {
        x.opt->confirm_with_error("output[] should only have 1 element in current version.");
        return ;
    }
    std::string touchfile = api.locale_path(
        x.opt->src_prefix + x.outputs[0] + x.opt->BUILD_ENTRY);
    std::string outfile = api.locale_path(x.opt->src_prefix + x.outputs[0]);

    std::string cmd;
    if (x.cfg["host_shell"] == "cmd") {
        cmd = "cmd.exe /c " + api.shell_escape(
            "certutil.exe -urlcache -split -f " + x.url + " " + outfile
        ) + " && cmd /c \"type nul >" + touchfile + "\" 1 > nul";
    }
    else if (x.cfg["host_shell"] == "powershell") {
        cmd = "powershell -Command \"Invoke-WebRequest -Uri '"
            + x.url + "' -OutFile '" + outfile + "'";
    }
    else if (x.cfg["host_shell"] == "bash" || x.cfg["host_shell"] == "zsh") {
        cmd = "curl -o " + api.shell_escape(outfile) + " " + x.url
            + "&& touch " + api.shell_escape(touchfile) + " 1> /dev/null 2>&1";
    }
    else {
        x.opt->confirm_with_error("unsupported shell");
        return ;
    }

    cgn::CGNTargetOpt *opt = x.opt->confirm();

    constexpr const char *rule = "@cgn.d//library/general.cgn.bundle/rule.ninja";
    static std::string rule_path = api.get_filepath(rule);
    opt->ninja->append_include(rule_path);

    auto *field = opt->ninja->append_build();
    field->rule = "quick_run";
    field->variables["cmd"]  = cmd;
    field->variables["desc"] = "DOWNLOAD " + x.url;
    field->outputs = {opt->ninja->escape_path(touchfile)};

    auto *phony = opt->ninja->append_build();
    phony->rule = "phony";
    phony->inputs  = field->outputs;
    phony->outputs = {opt->ninja->escape_path(opt->out_prefix + opt->BUILD_ENTRY)};

    opt->result.outputs = {outfile};
}
