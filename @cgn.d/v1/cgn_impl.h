#pragma once
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <mutex>
#include "graph.h"
#include "logger.h"
#include "configuration_mgr.h"
#include "cgn_type.h"
#include "dl_helper.h"

namespace cgnv1
{

class CGNImpl {
public:

    std::pair<GraphNode*, std::string> active_script(const std::string &label);

    void offline_script(const std::string &label);

    //@param label : factory label like '//hello/cpp1'
    //@param cfg   : configuration on specific target_factory
    //@param OUT adep_test : .ninja entry filename only for build_target()
    //                       (OS specific path-sep)
    CGNTarget analyse_target(
        const std::string &label, 
        const Configuration &cfg
    );

    // add file to obj_placeholder_ninja[]
    // In the inputs of a Ninja target, some files may not exist at build phase, 
    // and these files may be shared by multiple Ninja targets. Therefore, they 
    // are placed in a separate global Ninja file.
    // Reference:
    //   https://ninja-build.org/manual.html#_the_literal_phony_literal_rule
    void add_obj_file_placeholder(std::string str);

    // re-assign this flag for each 'round' calling.
    // 0 normal mode
    // 'b' the build_check mode: in confirm_target_opt(), if cache_result
    //     found or the target node 'Latest', return directly.
    // 'a' analyse only mode: no ninja_file pointer created.
    //                        (TODO: interpreter update)
    char current_analysis_level = 0;

    CGNTargetOpt *confirm_target_opt(CGNTargetOptIn *in);

    void add_adep(GraphNode *early, GraphNode *late);

    // Calling before other function called
    // * clear mtime cache (windows folder mtime)
    // * clear analyse recursion check stack
    void start_new_round();

    std::shared_ptr<void> bind_target_builder(
        const std::string &label,
        std::function<void(CGNTargetOptIn*)> loader
    );

    // @return target.outputs[0] and ninjabuild exitcode
    std::pair<std::string, int> build_target(
        const std::string &label, const Configuration &cfg);

    CGNImpl(std::unordered_map<std::string, std::string> cmd_kvargs);

    ~CGNImpl();

    std::string expand_filelabel_to_filepath(const std::string &in) const;

    std::unique_ptr<ConfigurationManager> cfg_mgr;
    std::unordered_map<std::string, std::string> cmd_kvargs;

    RuntimeEnv runtime_env;

    Logger logger;

private:
    // CGN *host_api;
    
    //@return pair<result, errmsg>
    std::pair<std::string, std::string> 
    _expand_cell(const std::string &ss) const;

    // std::unordered_map<std::string, std::string> cells;
    std::unordered_set<std::string> cells;

    // analysis_path : cgn_out/analysis_<os><cpu><dbg/rel>
    // obj_main_ninja : the ninja build entry
    // obj_placeholder_ninja : the file generating in build phase.
    // cgn_out : args from --cgn-out
    // cgn_out_unixsep : cgn_out with '/' unix-path-separator
    // cgnapi_winimp : (windows-only) input argument for @cgn.d/pe_loader
    // script_cc : args from --scriptcc
    // scriptcc_debug_mode : args from --scriptcc_debug
    // halt_on_error : args from --halt_on_error
    std::filesystem::path cgn_out;
    std::filesystem::path analysis_path;
    std::filesystem::path obj_main_ninja;
    std::filesystem::path obj_placeholder_ninja;
    std::string cgn_out_unixsep;
    std::string script_cc;
    std::string cgnapi_winimp;
    bool scriptcc_debug_mode = false;
    bool halt_on_error = false;

    std::unordered_set<std::string> adep_cycle_detection;

    // TODO: no thread-safe supported currently
    // std::recursive_mutex analyse_mtx;
    // std::shared_mutex    script_mtx;

    Graph graph;

    //scripts[ulabel]
    //  factories["//cgn.d/library/shell.cgn.cc"]
    struct CGNScript {
        std::string sofile;
        std::unique_ptr<DLHelper> sohandle;
        GraphNode *anode;
    };
    std::unordered_map<std::string, CGNScript> scripts;

    //factories[ulabel]
    //  factories["//hello:world"]
    std::unordered_map<std::string, std::function<void(CGNTargetOptIn*)>> factories;

    //targets[ulabel#cfg_id]
    //  targets["//hello:world#FFFE1234"]
    std::unordered_map<std::string, CGNTarget> targets;
    
    // targets entry (obj_main_ninja)
    std::unordered_set<std::string> main_subninja;

    // ninja phony target of file ('/' unix-path-separator) (obj_placeholder_ninja)
    std::unordered_set<std::string> placeholder_ninja;
};


} // namespace