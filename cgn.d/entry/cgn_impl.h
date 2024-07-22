#pragma once
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include "graph.h"
#include "configuration.h"
#include "dl_helper.h"
#include "../cgn.h"

namespace cgn
{

class CGNImpl {
public:

    CGNTarget analyse_target(
        const std::string &label, 
        const Configuration &cfg,
        std::string *adep_test = nullptr
    );

    const CGNScript &active_script(const std::string &label);

    void offline_script(const std::string &label);

    void add_adep(GraphNode *early, GraphNode *late);

    // Calling before other function called
    // * clear mtime cache (windows folder mtime)
    // * clear analyse recursion check stack
    void precall_reset();

    std::shared_ptr<void> bind_target_factory(
        const std::string &ulabel,
        CGNFactoryLoader loader
    );

    void build_target(
        const std::string &label, const Configuration &cfg);

    void init(std::unordered_map<std::string, std::string> cmd_kvargs);

    std::string expand_filelabel_to_filepath(const std::string &in) const;

    std::unique_ptr<ConfigurationManager> cfg_mgr;
    std::unordered_map<std::string, std::string> cmd_kvargs;


private:
    std::string _expand_cell(const std::string &ss) const;

    // std::unordered_map<std::string, std::string> cells;
    std::unordered_set<std::string> cells;

    std::filesystem::path cgn_out;
    std::filesystem::path analysis_path;
    std::filesystem::path cell_lnk_path;
    std::filesystem::path obj_main_ninja;
    std::string script_cc;
    std::string cgnapi_winimp;
    bool scriptcc_debug_mode = false;

    std::unordered_set<std::string> adep_cycle_detection;

    Graph graph;

    //scripts[ulabel]
    //  factories["//cgn.d/library/shell.cgn.cc"]
    std::unordered_map<std::string, CGNScript> scripts;

    //factories[ulabel]
    //  factories["//hello:world"]
    std::unordered_map<std::string, CGNFactoryLoader> factories;

    //targets[ulabel#cfg_id]
    //  targets["//hello:world#FFFE1234"]
    std::unordered_map<std::string, CGNTarget> targets;
    
    // targets entry
    std::unordered_set<std::string> main_subninja;
};


} // namespace