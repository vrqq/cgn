#pragma once
#include <memory>
#include <filesystem>
#include "../cgn.h"
#include "../providers/provider.h"
#include "dl_helper.h"
#include "configuration.h"

// // * create node if not existed
// // * remove node if no edge connected
// class Graph {
// public:

//     void add_edge(const std::string &from, const std::string &to);
//     void remove_node(const std::string &from);
// };

class CGNImpl {
    struct FileNode;
    struct CGNAnalyticsNode;
public:
    // @return: script updated (the build.ninja need to regenerate 
    //          which depend on that one)
    bool load_cgn_script(const std::string &label);

    // 
    void unload_cgn_script(const std::string &label);

    TargetInfos analyse(
        const std::string &tf_label, 
        const Configuration &plat_cfg
    );

    //TODO: return BuildResult
    // @return 'return value of ninja build'
    int build(
        const std::string &tf_label,
        const Configuration &plat_cfg
    );

    // void clean_all();

    // @return the lifetime of target factory label registered
    // * register to factories[tf_label]
    // * add dep-edge: tf_label -> file_label
    // std::shared_ptr<void> auto_target_factory(
    //     const std::string &file_label, 
    //     const std::string &target_name
    // );

    using TargetApplyFunc = std::function<TargetInfos(const Configuration&, TargetOpt)>;
    std::shared_ptr<void> auto_target_factory(
        const std::string &label, TargetApplyFunc fn_apply
    );

    void register_ninjafile(const std::string &ninja_file_path);

    //TargetOpt is defined in cgn.h
    //@param factory_label: //project1:hello
    TargetOpt get_target_dir(const std::string &factory_label, ConfigurationID hash_id);

    std::vector<std::string> get_script_filepath(const std::string &label);

    CGNImpl(): ref_scripts(scripts) {}
    ~CGNImpl() {}
    void init(std::unordered_map<std::string, std::string> cmd_kvargs);

    std::unique_ptr<ConfigurationManager> cfg_mgr;
    std::unordered_map<std::string, std::string> cmd_kvargs;

    const std::unordered_map<std::string, FileNode> &ref_scripts;

private:
    constexpr static const char NINJA_NO_WORK_TO_DO[] = "ninja: no work to do.";
    
    //filled by init()
    std::filesystem::path cgn_out;
    std::filesystem::path analysis_path;
    std::filesystem::path cell_lnk_path;
    std::string           scriptc_ninja;
    std::string           obj_main_ninja;
    bool               regen_all = false;

    // std::string cgn_out_dir = "cgn-out";
    // std::filesystem::path file_cgncc_ninja;

    // entry of all target ninja
    // parse cgn-out/obj/main.ninja and load this one
    std::unordered_set<std::string> main_subninja;

    std::unordered_map<std::string, std::string> cells;

    //Base node in analystics graph
    struct CGNAnalyticsNode {
        enum NodeType{
            File, TargetFactory
        }node_type;
        std::vector<CGNAnalyticsNode *> from, to;
        CGNAnalyticsNode(NodeType _t) : node_type(_t) {}
    };

    //CGN-Script-File Node
    struct FileNode : CGNAnalyticsNode {
        std::shared_ptr<DLHelper> handle;
        // bool flag_setup = false;
        std::vector<std::string> files;

        FileNode() : CGNAnalyticsNode(File) {}
    };

    //CGN-Target-Factory Node
    struct TargetFactoryNode : CGNAnalyticsNode {
        TargetApplyFunc fn_apply;
        bool flag_no_cache = false;
        // std::unordered_map<std::string, TargetInfos> cache;

        TargetFactoryNode() : CGNAnalyticsNode(TargetFactory) {}
    };

    //scripts[<rfile of cgn-workspace-root>]
    std::unordered_map<std::string, FileNode> scripts;

    //factories[<rfile of cgn-workspace-root>]
    std::unordered_map<std::string, TargetFactoryNode> factories;

    // clear before external call
    std::unordered_set<std::string> analytics_file_visited;

    std::unordered_set<std::string> analytics_stack;

    struct ScriptLabel {
        //relavent path of working root
        // filepath_in   : 'project1/BUILD.cgn.cc'
        // ninjapath_mid : 'cgn-out/analytics/project1/BUILD.cgn.ninja'
        // libpath_out   : 'cgn-out/analytics/project1/libBUILD.cgn.so'
        std::filesystem::path filepath_in, ninjapath_mid, libpath_out;

        //the pattern project to '//' without cellname
        // //project1/BUILD.cgn.cc
        std::string unique_label;

        // -D when build file.cgn.cc
        // Note: shell escape has been considered
        std::string def_var_prefix;
        std::string def_ulabel_prefix;
        
        // '/' in linux and '\\' in windows
        // std::string path_separator;
    };
    //@param label: BUILD.gcn.cc or xxx.cgn_deps
    //              //cgn.d/library/lang_rust.cgn.rsp
    //              //project1/BUILD.cgn.cc
    ScriptLabel parse_script_label(const std::string &label);


    // struct TargetLabel {
    //     // cgn_file    : 'project1/BUILD.cgn.cc'
    //     // target_file : 'cgn-out/obj/project1_/hello_12CDEF/build.ninja'
    //     // std::filesystem::path cgn_file, target_ninja_file;
        
    //     //[CGNImpl::analyse]
    //     //the pattern project to '//' without cellname
    //     //  //project1/BUILD.cgn.cc
    //     std::string script_unique_label;

    //     //[CGNImpl::analyse] [interpreter: ContextArgs]
    //     //the pattern project to '//' without cellname
    //     //  //project1:hello
    //     std::string tf_unique_label;

    //     //[CGNImpl::build] [CGNImpl::analyse] [interpreter: _2]
    //     //dir for build phase
    //     //  build.ninja      : cgn-out/obj/project1_/hello_12CDEF/build.ninja
    //     //  target-main      : cgn-out/obj/project1_/hello_12CDEF/.build_stamp
    //     //  change-detection : cgn-out/obj/project1_/hello_12CDEF/.analysis_stamp
    //     std::string target_dir;
    // };

    //@param label: //third_party/fmt
    //              //base:unicode
    // TargetLabel parse_tf_label(const std::string &label, Configuration plat_cfg);

    //Internal only, expand ss with cell without '//' prefix.
    // use by parse_script_label() and get_target_dir() 
    std::string _expand_cell(const std::string &ss);

    //generate cgn-out/obj/main.ninja
    void prepare_main_ninja();

    //generate cgn-out/analytics/.cgn.ninja
    void prepare_scriptc_ninja(std::string cc = "");

    // generate folder symbolic link inside "cgn-out/script_include/"
    void cgn_cell_init();
};
