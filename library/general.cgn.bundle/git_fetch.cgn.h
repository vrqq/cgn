#pragma once
#include <string>
#include "../../cgn.h"
#include "windef.h"

// Git DEPOT
// ---------
struct GitContext
{
    const std::string name;
    
    //keep empty to use system git, or using tools like
    //  "@cgn.d//git_depot" (TODO: dep unsolved)
    std::string using_depot_tool;

    // the source where git to
    std::string dest_dir = "repo";
    
    std::string repo;
    std::string commit_id;

    bool fetch_submodule = false;

    struct {
        std::vector<std::string> command;
        std::string cwd = ".";
    }post_script;

    GitContext(cgn::CGNTargetOptIn *opt)
    : name(opt->factory_name), opt(opt) {}

private: friend struct GitFetcher;
    cgn::CGNTargetOptIn *opt;
};

struct GitFetcher
{
    using context_type = GitContext;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }
    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
};

#define git(name, x) CGN_RULE_DEFINE(GitFetcher, name, x)


// HTTP Downloader

// struct UnarchiveContext
// {
//     std::string ;
//     std::string download_to;
// };