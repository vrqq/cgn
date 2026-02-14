// CGN Public API
//   Provide a global variable 'api'.

#pragma once
#include <string>
#include <memory>
#include <functional>
#include "cgn_type.h"
#include "logger.h"

// Compiler barrier macro
#if defined(_MSC_VER) // MSVC
    #include <intrin.h>
    #define COMPILER_BARRIER() _ReadWriteBarrier()
#elif defined(__GNUC__) || defined(__clang__) // GCC, Clang
    #define COMPILER_BARRIER() asm volatile("" ::: "memory")
#else
    #define COMPILER_BARRIER() // Fallback: No-op
#endif

namespace cgnv1 {

struct CGN_EXPORT Tools {
    static uint32_t host_to_u32be(uint32_t in);
    static uint32_t u32be_to_host(uint32_t in);

    // @param shell_type: cmd, powershell, bash
    static std::string shell_escape(
        const std::string &in,
        const std::string &shell_type
    );

    static HostInfo get_host_info();

    // TBD: Do we need to expose these advcopy tool?
    struct PatchSearchResult {
        std::string PATH_SEPARATOR;

        // only regular_file, symlink_file or empty_dir
        // pair<path matched pattern, the relative path of matched path (empty allowed)>
        // the full path need to copy: {pair.first / pair.second}
        std::vector<std::pair<std::string, std::string>> file_need_copy;

        // regular_file, symlink_file or directory
        // node add to depfile
        std::vector<std::string> node_need_watch;

        std::string errmsg;
    };

    // @return result
    static PatchSearchResult path_search(const std::string &pattern);

    static std::vector<std::string> file_glob(const std::string &dir, std::string *errmsg = nullptr);
    
    static std::string copy_to_dir(
        const std::vector<std::string> &src_rel, 
        const std::vector<std::string> &src_rel_exclude, 
        const std::string &src_base, 
        const std::string &dst_dir,
        const std::string depfile,
        const std::string stampfile,
        bool print_log = false
    );

    static std::string flatcopy_to_dir(
        const std::vector<std::string> &src, 
        const std::vector<std::string> &src_exclude, 
        const std::string &dst_dir,
        const std::string depfile,
        const std::string stampfile,
        bool print_log = false
    );

    static std::string copy_rename(
        const std::string &src,
        const std::string &dst,
        const std::string &depfile,
        const std::string &stampfile,
        bool print_log = false
    );

    // converts p to be relative to a different base directory.
    // The path returned is converted into weakly_canonical format,
    // i.e. 'c:\windows' to 'C:\windows'
    //
    // @param new_base: 
    //   The directory to convert the paths to be relative to. This can be an
    //   absolute path or a relative path (which will be treated as being relative
    //   to the current BUILD-file's directory).
    //   As a special case, if new_base is the empty string (the default), all
    //   paths will be converted to system-absolute native style paths with system
    //   path separators. This is useful for invoking external programs.
    //   If both p and new_base are absolute path, it will try to generate a relative
    //   path visited from new_base, if no relative exsit return p directly.
    // @param current_base: 
    //   The current location of path `p`.
    //   If `p` is an absolute path, `current_base` is ignored.
    static std::string rebase_path(
        const std::string &p, 
        const std::string &new_base, 
        const std::string &current_base = "."
    );


    // expand CGNPath to relative path of working-root or absolute path
    // * if p.rpath is absoulte path, return directly.
    // * If p.type == BASE_ON_OUTPUT : only CGNTargetMaker can be used.
    // * If p.type == BASE_ON_WORKINGROOT : the param 'opt' is ignored.
    // @param p : path in
    // @param opt : current environment
    // @param new_base : The directory to convert the paths to be relative to. 
    //          if new_base is the empty string, absolute path returned.
    static std::string rebase_path(
        const CGNPath &p,
        const std::string &new_base,
        CGNTargetOpt *opt
    );
    static std::string rebase_path(
        const CGNPath &p,
        const std::string &new_base,
        CGNTargetMaker *maker
    );

    static CGNPath convert_cgnpath_to_working_root(
        const CGNPath &p,
        CGNTargetOpt *opt
    );
    static void convert_cgnpath_to_working_root_inplace(
        std::vector<CGNPath> &ls,
        CGNTargetOpt *opt
    );

    // convert path 'in' to OS-dependent separator style, even if the path does 
    // not exist. remove section which name '.'(dot) at begin or end.
    // The final '/' will be kept to indicate that is directory.
    // For windows, convert "c:" to "C:" (uppercase)
    // 
    // Example:
    //   "/" => "/"
    //   "c:\win" => "C:\win"
    //  "./file"  => "file"
    //  "dir/"    => "dir/"
    //  "./dir/." => "dir/"
    //   "." or "./" => "" (unnecessary dot)
    //
    static std::string locale_path(const std::string &in);

    // Retrieve the parent path of the input, accepting both absolute and 
    // relative paths. If the parent path does not exist, return ".".
    static std::string parent_path(const std::string &in);

    // Retrive filename of the input
    static std::string filename_of_path(const std::string &in);

    static std::string extension_of_path(const std::string &in);

    // checking 'p' is inside 'dir' or not
    static bool is_file_inside(const std::string &p, const std::string &dir);

    static std::unordered_map<std::string, std::string> read_kvfile(
        const std::string &filepath);

    // read whole file into std::string
    static std::string read_wholefile(
        const std::string &filepath, bool exception_if_not_found = false);

    // update file content (mtime update implied) if data modified
    // @return file written
    static bool write_file_content_if_changed(
        const std::string &filepath, const std::string &content);

    // get mtime of specific file
    static int64_t stat(const std::string &filepath);

    // Get mtime for all files in folder
    static std::unordered_map<std::string, int64_t> win32_stat_folder(
        const std::string &folder_path);

    // Checks if the given file status or path corresponds to a regular file.
    static bool is_regular_file(const std::string &path);

    // Checks if path is absolute or not
    static bool is_absolute_path(const std::string &path);

    // Escape char like ':', '\', '..' to make path valid for all OS
    // @param alter_prefix: the prefix for return value, 
    //                      if path is absolute path, this param is ignored and use 'A' instead.
    //                      throw exception if alter_prefix == 'A'
    // @return OS-perferred-sep relative path
    //         for absolute path, add 'A' prefix, otherwise 's' for SCRIPTbase, 'c' for WorkingRootBase
    // e.g. 
    //   c:\windows => AC_3A_\windows     (host is WIN, abspath)
    //   .\C:\win   => <prefix>C_3A_\win  (host is WIN, path on remote, relpath)
    //   D:\efg/hij => <prefix>D_3A_5Cefg_/hij  (host is UNIX, relpath)
    //   /dir/f2    => Adir_/f2                 (host is UNIX, abspath)
    // for all platforms :
    //   str1    => str1
    //   ./name2 => name2
    //    _/x        => __/x
    //    ../file    => .._/file
    //    A../file   => A.._/file
    //    ./././a/b/c => a_/b_/c
    static std::string mangle_path_to_relative(const std::string &cpath, const char alter_prefix = 'R');

    //@depecated
    static std::string mangle_path(const std::string &file, const std::string &base);
    // static bool is_absolute_path(const std::string &path);

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

    // @param p : usually src_prefix, like './@cell1/dir1/', ...
    // @return : default target on specific dir like "@cell1//dir1", ...
    // static std::string convert_path_to_label(const std::string &p);

    static bool setenv(const std::string &key, const std::string &value);

    static std::string getenv(const std::string &key);

    //return parent process name, for example
    // windows: "cmd.exe" or "powershell.exe"
    // linux: "bash", "zsh" or "fish"
    static std::string get_parent_process_name();

    static bool is_directory_case_sensitive(const std::string& directory);

    // remove duplicate items in list.
    static void remove_duplicate_inplace(std::vector<std::string> &data, bool front_to_end = true);

    template<typename T, typename Handle> std::string convert_list_to_string(
        const T &ls, Handle escaper = [](const std::string &in){ return in; }
    ) {
        std::string rv;
        for (auto &it : ls)
            rv += (rv.empty()?"":" ") + escaper(it);
        return rv;
    }

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
    std::string offline_script(const std::string &label);

    std::string add_factory(
        const std::string &factory_label,
        std::function<void(CGNTargetOpt*)> loader
    );

    std::string remove_factory(
        const std::string &factory_label
    );

    // Analyse specific target
    CGNTarget create_target(
        const std::string &label, const Configuration &cfg
    );

    CGNTarget create_target(
        CGNTargetOpt *opt_in, const std::function<void(CGNTargetOpt*)> &loader
    );

    // Build specific target
    std::string build(const std::string &label, const Configuration &cfg);

    // Query named configuration assigned in cgn_setup.cgn.cc
    std::pair<Configuration, GraphNode *>
    query_config(const std::string &name) const;
    
    // Manually add an analyse dependency edge from 'early' to 'late' in Graph.
    void add_adep_edge(GraphNode *early, GraphNode *late);

    template<typename Interpreter, typename ...Preloads> std::shared_ptr<void> 
    bind_target_factory(
        const std::string &factory_name,
        std::function<void(typename Interpreter::context_type&)> factory,
        Preloads ...labels
    ) {
        std::string factory_label = get_debug_runtime()->label + ":" + factory_name;
        std::array<const char*, sizeof...(labels)> extra_scripts{labels...};
        auto loader = [this, factory, extra_scripts](CGNTargetOpt *opt) {
            // load prerequisite
            for (const char *label : Interpreter::preload_labels() + extra_scripts) {
                if (label == nullptr)
                    throw std::runtime_error{"Wrong preload_labels()"};
                std::pair<cgnv1::GraphNode *, std::string> dll = active_script(label);
                if (dll.second.size())
                    return opt->set_fail(dll.second);
            }

            // prevent compiler reorder the functions.
            COMPILER_BARRIER();

            // prepare Interpreter::Context, call factory, then interpreter.
            typename Interpreter::context_type x{opt};
            factory(x);
            Interpreter::interpret(x);
        };
        auto errmsg = add_factory(factory_label, loader);
        if (errmsg.size())
            throw std::runtime_error{errmsg};
        return std::shared_ptr<void>(nullptr, 
            [this, factory_label](void*) mutable{
                remove_factory(factory_label); 
            });
    }

    // The init function must be called before others.
    // @param kvargs : 
    //          kvargs["cgn-out"] = OS-perferred-path-string of output path (requirement)
    //          kvargs["verbose"] : enable verbose mode
    //          kvargs["scriptcc_debug"] : enable debug mode for ScriptCC
    //          kvargs["halt_on_error"] : exit when analyse_target() return error
    //          kvargs["winenv"] : call vcvars64.bat before ScriptCC (cl.exe)
    //
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

    const TLRuntime *get_debug_runtime() const;

    std::string get_cgn_binary_mirror_path() const;

    ~CGN();

    Logger *logger = nullptr;

private:
    CGNImpl *pimpl = nullptr;
}; //class CGN

} //namespace


CGN_EXPORT extern cgnv1::CGN api;
