#define CGN_LIBRARY_GIT_IMPL
#include "git_fetch.cgn.h"

CGN_LIBRARY_GIT_API void GitFetcher::interpret(context_type &x)
{
    if (x.using_depot_tool.size())
        return x.opt->set_fail("'using_depot_tool' current unavailable.");

    bool is_win_host = (x.opt->cfg["host_os"] == "win");
    std::string shell_type = x.opt->cfg["host_shell"];
    auto two_escape = [shell_type, is_win_host](const std::string &in) {
        return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in, is_win_host?"cmd":shell_type));
    };
    x.opt->cfg.visit_keys({"host_os"});

    // depot_tools preload 
    std::string depot_tool_exe;
    if (x.using_depot_tool.size()) {
        auto tool = x.quick_dep_namedcfg(x.using_depot_tool, "host_release", false);
        if (tool.errmsg.size())
            return x.opt->set_fail(tool.errmsg);
        depot_tool_exe = tool.outputs[0];
    }

    cgn::CGNTargetMaker *mk = x.opt->confirm();
    if (!mk)
        return ;
    
    std::string dest_dir = api.rebase_path(x.dest_dir, ".", mk);
    mk->outputs = {dest_dir};

    // Dependency of git dest_dir output (empty configuration, no visit_keys required.)
    // $x.out_parent_dir + $x.name + "_00000000"
    cgn::CGNTargetOpt opt_git_out = *x.opt;
    opt_git_out.cfg = cgn::Configuration{};
    cgn::CGNTarget git_out = api.create_target(&opt_git_out, 
    [dest_dir](cgn::CGNTargetOpt *opt){
        cgn::CGNTargetMaker *mk = opt->confirm();
        if (!mk || !mk->ninja)
            return ;
        auto *field = mk->ninja->append_build();
        field->rule = "phony";
        field->outputs = {cgn::NinjaFile::escape_path(mk->ninja_entry = dest_dir)};
        
        field = mk->ninja->append_build();
        field->rule = "phony";
        field->outputs = {cgn::NinjaFile::escape_path(dest_dir + "/.git")};

        field = mk->ninja->append_build();
        field->rule = "phony";
        field->outputs = {cgn::NinjaFile::escape_path(dest_dir + "/.git/HEAD")};
    });

    if (mk->ninja == nullptr)
        return ;

    constexpr const char *rule = "@cgn.d//library/utility/quick_run.ninja";
    static std::string rule_path = api.get_filepath(rule);

    mk->ninja->append_include(rule_path);
    auto *field = mk->ninja->append_build();
    
    if (x.using_depot_tool.size()) {
        std::string cmd = two_escape(depot_tool_exe) 
                        + " --stamp " + two_escape(mk->ninja_entry)
                        + " --dir " + two_escape(dest_dir)
                        + " " + x.repo + " " + x.commit_id;
        field->variables["cmd"] = cmd;
        field->implicit_inputs = {cgn::NinjaFile::escape_path(git_out.ninja_entry)};
        field->outputs = {mk->ninja->escape_path(mk->ninja_entry)};
        field->rule = "quick_run";
    }
    else {
        std::string cmd;
        if (mk->trimmed_cfg["host_os"] == "win") {
            std::string cmd_true = "echo .>NUL";
            cmd = std::string{"cmd.exe /c \""}
                + "(mkdir " + two_escape(dest_dir) + " || " + cmd_true + ")"
                + " && pushd " + two_escape(dest_dir)
                + " && (git init || " + cmd_true + ")"
                + " && (git remote add origin " + x.repo + " || " + cmd_true + ")"
                + " && git fetch --depth=1 --recurse-submodules=on-demand origin " + x.commit_id
                + " && git reset --hard " + x.commit_id
                + " && popd"
                + " && type nul > " + two_escape(mk->ninja_entry)
                + "\"";
        }
        else { // linux, unix sh
            std::string cmd_true = "true";
            cmd = "mkdir -p " + two_escape(dest_dir)
                + " && pushd " + two_escape(dest_dir)
                + " && (git init || " + cmd_true + ")"
                + " && (git remote add origin " + x.repo + " || " + cmd_true + ")"
                + " && git fetch --depth=1 --recurse-submodules=on-demand origin " + x.commit_id
                + " && git reset --hard " + x.commit_id
                + " && popd " //cdback 
                + " && touch " + mk->ninja_entry + " 1> /dev/null 2>&1";
        }

        // cmd + x.post_script
        if (x.post_script.command.size()) {
            if (!x.post_script.cwd.empty()){
                std::string path1 = api.rebase_path(x.post_script.cwd, ".", mk);
                cmd += " && pushd " + path1;
            }
            cmd += " &&";
            for (auto arg : x.post_script.command)
                cmd += " " + two_escape(arg);
            cmd += " && popd";
        }

        // write $cmd to build.ninja
        field->variables["cmd"] = cmd;
        field->variables["desc"] = "GIT FETCH " + x.repo;
        field->implicit_inputs = {cgn::NinjaFile::escape_path(git_out.ninja_entry)};
        field->outputs = {mk->ninja->escape_path(mk->ninja_entry)};
        field->rule = "quick_run";
    }
} //GitFetcher::interpret()
