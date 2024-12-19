// CGN Public API
//   Provide a global variable 'api'.

#pragma once
#include <string>
#include <memory>
#include <functional>
#include "cgn_type.h"
#include "logger.h"

namespace cgnv1 {


struct CGN_EXPORT Tools {
    static uint32_t host_to_u32be(uint32_t in);
    static uint32_t u32be_to_host(uint32_t in);

    static std::string shell_escape(
        const std::string &in
    );

    static HostInfo get_host_info();

    static std::vector<std::string> file_glob(const std::string &dir, const std::string &base = ".");

    // converts p to be relative to a different base directory.
    // @param new_base: 
    //   The directory to convert the paths to be relative to. This can be an
    //   absolute path or a relative path (which will be treated as being relative
    //   to the current BUILD-file's directory).
    //   As a special case, if new_base is the empty string (the default), all
    //   paths will be converted to system-absolute native style paths with system
    //   path separators. This is useful for invoking external programs.
    // static std::string rebase_path(const std::string &p, const std::string &new_base);    
    static std::string rebase_path(
        const std::string &p, 
        const std::string &new_base, 
        const std::string &current_base = "."
    );

    // convert path 'in' to OS-dependent separator style, even if the path does 
    // not exist.
    static std::string locale_path(const std::string &in);

    // Retrieve the parent path of the input, accepting both absolute and 
    // relative paths. If the parent path does not exist, return ".".
    static std::string parent_path(const std::string &in);

    static std::unordered_map<std::string, std::string> read_kvfile(
        const std::string &filepath);

    // get mtime of specific file
    static int64_t stat(const std::string &filepath);

    // Get mtime for all files in folder
    static std::unordered_map<std::string, int64_t> win32_stat_folder(
        const std::string &folder_path);

    // Checks if the given file status or path corresponds to a regular file.
    static bool is_regular_file(const std::string &path);

    // Creates a directory.
    static void mkdir(const std::string &path);

    //@param mode: +w, -r, u+rwx, go+rwx, 0755
    static void set_permission(const std::string &file, std::string mode);

    static bool win32_long_paths_enabled();

    static bool is_win7_or_later();

    //@param p : label like ':lib1', "../:lib1", "//other_part", "../../pkg"
    //@param base : label based on (like "//hello/cpp1")
    //@return : <base><p> (like //hello/cpp1:lib1)
    static std::string absolute_label(const std::string &p, std::string base);

    static bool setenv(const std::string &key, const std::string &value);

    static std::string getenv(const std::string &key);

    // remove duplicate items in list.
    static void remove_duplicate_inplace(std::vector<std::string> &data);

    static std::string get_lowercase_extension(const std::string &filename);

    static int win_copy(const std::string &src, const std::string &dst);

}; //struct Tools

class CGNImpl;

class CGN_EXPORT CGN : public Tools {
public:
    std::string get_filepath(const std::string &file_label) const;

    // Clear all mtime cache, rescan all files to check which is changed and reload them.
    void start_new_round();

    // load CGNScript, auto rebuild if necessary.
    std::pair<GraphNode*, std::string>
    active_script(const std::string &label);

    // unload CGNScript, it's safe to delete dll file after return.
    void offline_script(const std::string &label);

    // Analyse specific target
    CGNTarget analyse_target(
        const std::string &label, const Configuration &cfg
    );

    // Build specific target
    void build(const std::string &label, const Configuration &cfg);

    // ConfigurationID commit_config(const Configuration &plat_cfg);

    // Query named configuration assigned in cgn_setup.cgn.cc
    std::pair<Configuration, GraphNode *>
    query_config(const std::string &name) const;
    
    void add_adep_edge(GraphNode *early, GraphNode *late);

    template<typename Interpreter> std::shared_ptr<void> bind_target_factory(
        const std::string &factory_label,
        std::function<void(typename Interpreter::context_type&)> factory
    ) {
        auto loader = [this, factory](CGNTargetOptIn *opt) {
            // load prerequisite
            for (const char *label : Interpreter::preload_labels()) {
                std::pair<cgnv1::GraphNode *, std::string> dll = active_script(label);
                if (dll.second.size()) {
                    ((CGNTargetOpt*)opt)->result.errmsg = dll.second;
                    return ;
                }
                opt->quickdep_early_anodes.push_back(dll.first);
            }

            // prepare Interpreter::Context, call factory, then interpreter.
            typename Interpreter::context_type x{opt};
            factory(x);
            Interpreter::interpret(x);
        };
        return bind_target_builder(factory_label, loader);
    }

    std::shared_ptr<void> bind_target_builder(
        const std::string &factory_label, 
        std::function<void(CGNTargetOptIn*)> loader
    );

    // Commit from CGNTargetOptIn.confirm();
    CGNTargetOpt *confirm_target_opt(CGNTargetOptIn *in);

    // The init function must be called before others.
    void init(const std::unordered_map<std::string, std::string> &kvargs);

    // Make sure to call this function prior to ~CGN(), as the CGN API is an 
    // exported global variable that will be automatically deleted when the 
    // main-exe exits. Even though there are still many DLLs loaded, the static
    // variables within them can still call the API and SymbolTable during DLL
    // auto unload. 
    // Since the destruction order is not guaranteed, it is necessary to call 
    // release() in order to unload all DLLs before the program exits.
    void release();

    // Return kvargs assigned from init().
    const std::unordered_map<std::string, std::string> &get_kvargs() const;

    ~CGN();

    Logger *logger = nullptr;

private:
    CGNImpl *pimpl = nullptr;

}; //class CGN

} //namespace


CGN_EXPORT extern cgnv1::CGN api;
