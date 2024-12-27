#define GENERAL_CGN_BUNDLE_IMPL
#include "git_fetch.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

GENERAL_CGN_BUNDLE_API void GitFetcher::interpret(context_type &x)
{
    cgn::CGNTargetOpt *opt = x.opt->confirm();
    if (opt->cache_result_found)
        return ;

    constexpr const char *rule = "@cgn.d//library/general.cgn.bundle/rule.ninja";
    static std::string rule_path = api.get_filepath(rule);

    opt->ninja->append_include(rule_path);
    auto *field = opt->ninja->append_build();
    
    std::string dest_dir = api.locale_path(opt->src_prefix + x.dest_dir);
    if (false && x.using_depot_tool.size()) {
        auto host_cfg = api.query_config("host_release");
        if (host_cfg.second == nullptr) {
            opt->result.errmsg = "config 'host_release' not found.";
            return ;
        }
        api.add_adep_edge(host_cfg.second, opt->anode);
        auto tool = api.analyse_target(x.using_depot_tool, host_cfg.first);
        if (tool.errmsg.size()) {
            opt->result.errmsg = "depot_tool: " + tool.errmsg;
            return ;
        }
        std::string stamp = opt->out_prefix + opt->BUILD_ENTRY;
        std::string cmd = two_escape(tool.outputs[0]) 
                        + " --stamp " + two_escape(stamp)
                        + " --dir " + two_escape(dest_dir);
                        + " " + x.repo + " " + x.commit_id;
        field->variables["cmd"] = cmd;
        field->outputs = {opt->ninja->escape_path(stamp)};
        field->rule = "quick_run";
    }
    else {
        std::string touchfile = api.locale_path(
            opt->src_prefix + x.dest_dir + opt->BUILD_ENTRY);
        #ifdef _WIN32
        std::string cmd_true = "echo.>NUL";
        std::string suffix = "cmd /c \"type nul >" + touchfile + "\""
                           + " 1 > nul";
        #else
        std::string cmd_true = "true";
        std::string suffix = "touch " + touchfile
                           + " 1> /dev/null 2>&1";
        #endif
        
        std::string cmd = "mkdir -p " + two_escape(dest_dir)
                        + " && cd " + two_escape(dest_dir)
                        + " && (git init || " + cmd_true + ")"
                        + " && (git remote add origin " + x.repo + " || " + cmd_true + ")"
                        + " && git fetch --depth=1 origin " + x.commit_id
                        + " && git reset --hard " + x.commit_id
                        + " && cd " + api.rebase_path(".", dest_dir); //cdback

        // cmd + x.post_script
        if (x.post_script.command.size()) {
            std::string cdback;
            if (x.post_script.cwd.size()){
                std::string path1 = api.locale_path(opt->src_prefix + x.post_script.cwd);
                cdback = " && cd " + api.rebase_path(".", path1);
                cmd += " && cd " + path1;
            }
            cmd += " && ";
            for (auto arg : x.post_script.command)
                cmd += opt->ninja->escape_path(api.shell_escape(arg)) + " ";
            cmd += cdback;
        }

        //cmd + stampfile
        cmd += " && " + suffix;

        field->variables["cmd"] = cmd;
        field->variables["desc"] = "GIT FETCH " + x.repo;
        // field->outputs = {opt.ninja->escape_path(opt.src_prefix + opt.BUILD_ENTRY)};
        field->outputs = {touchfile};
        field->rule = "quick_run";

        auto *entry = opt->ninja->append_build();
        entry->rule = "phony";
        entry->outputs = {opt->out_prefix + opt->BUILD_ENTRY};
        entry->inputs  = {touchfile};
    }

    opt->result.outputs = {dest_dir};
}