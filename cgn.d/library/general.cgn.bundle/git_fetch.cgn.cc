#include "git_fetch.cgn.h"

static std::string two_escape(const std::string &in) {
    return cgn::NinjaFile::escape_path(cgn::CGN::shell_escape(in));
}

cgn::TargetInfos GitFetcher::interpret(context_type &x, cgn::CGNTargetOpt opt)
{
    constexpr const char *rule = "@cgn.d//library/general.cgn.bundle/rule.ninja";
    static std::string rule_path = api.get_filepath(rule);

    opt.ninja->append_include(rule_path);
    auto *field = opt.ninja->append_build();
    
    std::string dest_dir = api.locale_path(opt.src_prefix + x.dest_dir);
    if (x.using_depot_tool.size()) {
        auto tool = api.analyse_target(x.using_depot_tool, *api.query_config("host_release"));
        auto *def = tool.infos.get<cgn::DefaultInfo>();
        std::string stamp = opt.out_prefix + opt.BUILD_ENTRY;
        std::string cmd = two_escape(def->outputs[0]) 
                        + " --stamp " + two_escape(stamp)
                        + " --dir " + two_escape(dest_dir);
                        + " " + x.repo + " " + x.commit_id;
        field->variables["cmd"] = cmd;
        field->outputs = {opt.ninja->escape_path(stamp)};
        field->rule = "run";
    }
    else {
        std::string touchfile = api.locale_path(x.dest_dir + opt.BUILD_ENTRY);
        #ifdef _WIN32
        std::string suffix = "cmd /c \"type nul >" + touchfile + "\""
                           + " 1 > nul";
        #else
        std::string suffix = "touch " + touchfile
                           + " 1> /dev/null 2>&1";
        #endif
        std::string cmd = "mkdir -p " + two_escape(dest_dir)
                        + " && cd " + two_escape(dest_dir) + " && git init"
                        + " && git remote add origin " + x.repo
                        + " && git fetch --depth=1 origin " + x.commit_id
                        + " && git reset --hard " + x.commit_id
                        + " && cd .. && " + suffix;
        field->variables["cmd"] = cmd;
        field->variables["desc"] = "GIT FETCH " + x.repo;
        field->outputs = {opt.ninja->escape_path(opt.src_prefix + opt.BUILD_ENTRY)};
        field->rule = "run";

        auto *entry = opt.ninja->append_build();
        entry->rule = "phony";
        entry->outputs = {opt.out_prefix + opt.BUILD_ENTRY};
        entry->inputs  = {opt.src_prefix + opt.BUILD_ENTRY};
    }

    cgn::TargetInfos rv;
    auto *def = rv.get<cgn::DefaultInfo>(true);
    def->target_label = opt.factory_ulabel;
    def->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;
    return rv;
}