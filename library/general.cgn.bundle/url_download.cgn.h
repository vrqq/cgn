#pragma once
#include <vector>
#include <string>
#include "../../cgn.h"
#include "windef.h"

// HTTP DOWNLOAD
// -------------

struct URLDownloader
{
    struct context_type
    {
        const std::string &name;
        cgn::Configuration &cfg;

        std::string url;
        std::vector<std::string> outputs;
        
        context_type(cgn::CGNTargetOptIn *opt)
        : name(opt->factory_name), cfg(opt->cfg), opt(opt) {}

    private: friend struct URLDownloader;
        cgn::CGNTargetOptIn *opt;
    };

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/general.cgn.bundle"};
    }

    GENERAL_CGN_BUNDLE_API static void interpret(context_type &x);
};

#define url_download(name, x) CGN_RULE_DEFINE(URLDownloader, name, x)
