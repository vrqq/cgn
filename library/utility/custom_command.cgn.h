// create a custom build script
// NO-DEPS
#pragma once
#ifdef _WIN32
    #ifdef CGN_UTILITY_IMPL
        #define CGN_UTILITY_API  __declspec(dllexport)
    #else
        #define CGN_UTILITY_API
    #endif
#else
    #define CGN_UTILITY_API __attribute__((visibility("default")))
#endif

#include <cgn>

struct CustomCommand : cgn::QuickDepContext
{
    const std::string &name;
    cgn::Configuration &cfg;

    std::vector<cgn::CGNPath> watch_inputs, watch_orderonly, watch_outputs;

    // CGNTarget.result.outputs[]
    std::vector<cgn::CGNPath> analysis_outputs;
    
    // CGNTarget.result.infos[]
    cgn::InfoTable analysis_infos;

    // char ninja_dep_level = cgn::CGNTarget::NINJA_LEVEL_DYNDEP;

    // add_dep() usually called before opt_confirm()
    cgn::CGNTarget 
    add_dep(const std::string &label, const cgn::Configuration &cfg, ) {
        return opt->quick_dep(label, cfg);
    }
    cgn::CGNTarget 
    add_dep(const std::string &label, const std::string &cfg_name) {
        return opt->quick_dep_namedcfg(label, cfg_name, true);
    }

    void add_dep(cgn::GraphNode *anode) {
        opt->quickdep_early_anodes.push_back(anode);
    }

    std::string 
    rebase_path(const cgn::CGNPath &p, const std::string &new_base = ".") {
        return api.rebase_path(p, new_base, opt);
    }

    void opt_confirm_error(const std::string &errmsg) {
        return opt->confirm_with_error(errmsg);
    }

    bool opt_confirm_cached() {
        cfg.visit_keys({"host_os"});
        return opt->confirm()->cache_result_found;
    }
    
    // TODO:
    //   helper class ShellScriptWorker
    CGN_UTILITY_API void append_setenv(const std::string &key, const std::string &value);
    CGN_UTILITY_API void append_setenv(const std::unordered_map<std::string, std::string> &data);
    CGN_UTILITY_API void append_pushd(const cgn::CGNPath &path);
    CGN_UTILITY_API void append_popd();
    CGN_UTILITY_API void append_cmd(const std::vector<std::string> &args);
    CGN_UTILITY_API void append_escaped_cmd(const std::string &line);

    // TBD:
    //   helper function like auto-devel-info-generator
    //                        auto-cxxinfo-generator
    //                        pkgconfig-generator
    //  Do not use function to do, using XXWorker instead, like CopyWorker.
    // CGN_UTILITY_API void set_analysis_result();

    CustomCommand(cgn::CGNTargetOpt *opt)
    : cgn::QuickDepContext(opt), name(opt->name), cfg(opt->cfg) {}

private: friend struct CustomInterpreter;
    std::vector<std::pair<
        std::string, std::function<std::string(cgn::CGNTargetOpt *)>
    >> script_content;
};

struct CustomInterpreter
{
    using context_type = CustomCommand;

    constexpr static cgn::ConstLabelGroup<1> preload_labels() {
        return {"@cgn.d//library/utility/custom_command.cgn.cc"};
    }
    CGN_UTILITY_API static void interpret(context_type &x);
};

#define custom_command(name, x, ...) CGN_RULE_DEFINE(::CustomInterpreter, name, x, ## __VA_ARGS__)
