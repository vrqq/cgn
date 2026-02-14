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

    // Load specific dynamic library by label.
    // @param parallel_build_mode : a special mode which only build the script, and do not record TLRuntime, no return and no dlopen.
    // @return : pair{GraphNode*, error_message}
    std::pair<GraphNode*, std::string> active_script(const std::string &label, bool parallel_build = false);

    std::string offline_script(const std::string &label);

    std::string add_factory(
        const std::string &factory_label,   
        std::function<void(CGNTargetOpt*)> loader
    );

    std::string remove_factory(
        const std::string &factory_label
    );

    //Factory name must not be omitted when it inside a folder named with ':'
    //For example: label='@cell//folder:name/src:target'
    //  src_dir = './@cell/folder:name/src'
    //  target = "target"
    //Another example: label='@cell//folder'
    //  src_dir = './@cell/folder'
    //  target = "folder"
    //
    //@param label : factory label like '//hello/cpp1'
    //@param cfg   : configuration on specific target_factory
    //@param OUT adep_test : .ninja entry filename only for build_target()
    //                       (OS specific path-sep)
    CGNTarget create_target(
        const std::string &label, 
        const Configuration &cfg
    );

    // Create anonymous target
    // For example: 
    //   api.create_target(opt, loader = [](opt){
    //      return XInterpreter::interpret(XInterpreter::Context(opt)); 
    //   }, 
    //   {"@cell//LangX.cgn.cc"});
    CGNTarget create_target(
        CGNTargetOpt *in,
        std::function<void(CGNTargetOpt *in)> loader
    );

    template<bool ByFactory>
    CGNTarget _create_target_impl(
        const std::string &a_label_in, 
        const Configuration &a_cfg,
        CGNTargetOpt *b_opt_in,
        std::function<void(CGNTargetOpt *in)> b_fn_loader
    );

    CGNTargetMaker *confirm_target_opt(CGNTargetOpt *in, const std::string &with_errmsg);

    // @return target and ninjabuild exitcode
    std::pair<CGNTarget, int> create_and_build_target(
        const std::string &label, 
        const Configuration &cfg
    );

    // add file to obj_placeholder_ninja[]
    // In the inputs of a Ninja target, some files may not exist at build phase, 
    // and these files may be shared by multiple Ninja targets. Therefore, they 
    // are placed in a separate global Ninja file.
    // Reference:
    //   https://ninja-build.org/manual.html#_the_literal_phony_literal_rule
    // void add_obj_file_placeholder(std::string str);

    // re-assign this flag for each 'round' calling.
    // 0 normal mode
    // 'b' the build_check mode: in confirm_target_opt(), if cache_result
    //     found or the target node 'Latest', return directly.
    // 'a' analyse only mode: no ninja_file pointer created.
    //                        (TODO: interpreter update)
    // char current_analysis_level = 0;

    // CGNTargetOpt *confirm_target_opt(CGNTargetOptIn *in);

    // add_adep from anode which early loaded to late loaded
    // for example:
    //   create_target() { add_adep(script_anode, target_anode); }
    void add_adep(GraphNode *early, GraphNode *late);

    // Calling before other function called
    // * clear mtime cache (windows folder mtime)
    // * clear analyse recursion check stack
    // void start_new_round();

    // std::shared_ptr<void> bind_target_builder(
    //     const std::string &label,
    //     std::function<void(CGNTargetOpt*)> loader
    // );

    CGNImpl(std::unordered_map<std::string, std::string> cmd_kvargs);

    ~CGNImpl();

    std::string expand_filelabel_to_filepath(const std::string &in) const;

    std::unique_ptr<ConfigurationManager> cfg_mgr;
    std::unordered_map<std::string, std::string> cmd_kvargs;

    thread_local static TLRuntime *tls_runtime;
    static void tls_push(TLRuntime *rt) { rt->call_from = tls_runtime; tls_runtime = rt; }
    static void tls_pop(TLRuntime *if_thisone) { if (tls_runtime == if_thisone) tls_runtime = tls_runtime->call_from; }

    Logger logger;

    std::filesystem::path cgn_exe_shadow;

private:
    // CGN *host_api;

    //@return : true to infinite loop detected.
    // bool _check_infinite_loop(const std::string &current_key);
    
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
    // std::filesystem::path obj_placeholder_ninja;
    std::string cgn_out_unixsep;
    std::string script_cc;
    std::string cgnapi_winimp;
    bool scriptcc_debug_mode = false;
    bool halt_on_error = false;

    // std::unordered_set<std::string> adep_cycle_detection;

    // TODO: no thread-safe supported currently
    // std::recursive_mutex analyse_mtx;
    // std::shared_mutex    script_mtx;

    Graph graph;

    //visit from api.active_script() and api.offline_script()
    //scripts[file_label]
    //  not_exist : active_script(label) hasn't been called
    //  exist with empty value : compiling
    //  exist with value : after dlopen and in mutex.
    // building : scripts[file_label] not exist, then dlopen and init global-variable
    // loaded   : (after global variable inited) assign scripts[file_label]
    //  factories["//cgn.d/library/shell.cgn.cc"]
    struct CGNScript {
        std::string sofile;
        std::unique_ptr<DLHelper> sohandle;
        GraphNode *anode;
    };
    // std::mutex scripts_mtx;
    std::unordered_map<std::string, CGNScript> scripts;
    std::mutex parallel_active_script_mutex;

    //factories[ulabel]
    //  factories["//hello:world"]
    struct NamedFactory {
        CGNScript *from_script;
        std::function<void(CGNTargetOpt*)> loader;
    };
    // std::mutex factories_mtx;
    std::unordered_map<std::string, std::function<void(CGNTargetOpt*)>> named_factories;
    // std::unordered_map<std::string, NamedFactory> named_factories;

    //targets[out_prefix]
    //  targets["cgn-out/obj/hello_world_00000000/"]
    std::unordered_map<std::string, CGNTarget> targets;
    
    // targets entry (obj_main_ninja)
    std::unordered_set<std::string> main_subninja;

    // ninja phony target of file ('/' unix-path-separator) (obj_placeholder_ninja)
    // std::unordered_set<std::string> placeholder_ninja;
};


} // namespace