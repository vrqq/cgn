#pragma once
#include <string>
#include "../../cgn.h"
#include "../../rule_marco.h"
#include "../../provider_dep.h"

// Git DEPOT
// ---------
struct GitContext
{
    const std::string name;
    
    //keep empty to use system git, or using tools like
    //  "@cgn.d//git_depot"
    std::string using_depot_tool;

    // the source where git to
    std::string dest_dir = "src";
    
    std::string repo;
    std::string commit_id;

    GitContext(const cgn::Configuration &cfg, cgn::CGNTargetOpt opt)
    : name(opt.factory_name) {}
};

struct GitFetcher
{
    using context_type = GitContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }
    static cgn::TargetInfos interpret(context_type &x, cgn::CGNTargetOpt opt);
};

#define git(name, x) CGN_RULE_DEFINE(GitFetcher, name, x)


// HTTP Downloader

// struct UnarchiveContext
// {
//     std::string ;
//     std::string download_to;
// };