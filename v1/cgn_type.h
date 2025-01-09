// cgn.d public
// === CGN Public Type define ===
//
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "api_export.h"
#include "ninja_file.h"
#include "configuration.h"

namespace cgnv1 {

class CGN;

//
// cgn_init functions
struct CGNInitSetup {
    //platforms[platform_name] = kv-pair
    std::unordered_map<std::string, Configuration> configs;

    //cfg_restrictions[key] = value[]
    std::unordered_map<std::string, std::vector<std::string>> cfg_restrictions;

    std::string log_message;
};

// only pointer type used.
// Node in CGN-Analysis-Graph.
struct GraphNode;

// FileLayout[rel_path_of_output_dir] = path_to_origin_file_or_folder,
// the value accept both the relavent path of working root and absolute path.
using FileLayout = std::map<std::string, std::string>;

struct HostInfo {
    //os : win, linux, mac
    //cpu: x86, x64, arm64
    std::string os, cpu;

    // gnu_get_libc_version() : 2.8
    // gnu_get_libc_release() : stable
    std::string glibc_version, glibc_release;
}; //struct HostInfo

// TBD
struct RuntimeEnv {
    std::string src_prefix;
};

struct BaseInfo
{
    struct VTable {
        std::shared_ptr<BaseInfo> (*allocate)();
        bool        (*merge_entry)(void *ecx, const BaseInfo *rhs);
        std::string (*to_string)(const void *ecx, char type);
    };

    BaseInfo(const VTable *vtable) : vtable(vtable) {};

    std::shared_ptr<BaseInfo> allocate() const {
        return vtable->allocate();
    }

    bool merge_entry(const BaseInfo *rhs) {
        return vtable->merge_entry(this, rhs);
    }
    std::string to_string(char type) const {
        return vtable->to_string(this, type);
    }

protected:
    const VTable *vtable = nullptr;
}; //struct BaseInfo

// predefined type
struct LinkAndRunInfo : BaseInfo {
    //The file path relavent to WorkingRoot
    std::vector<std::string> shared_files, static_files, object_files;
    
    // The files which not link directly but required when running.
    // It may the indirect dependency, and folder path also accepted.
    FileLayout runtime_files;

    LinkAndRunInfo() : BaseInfo{&v} {}
    
    static const char *name() { return "LinkAndRunInfo"; }

private:
    CGN_EXPORT static const VTable v;
}; //struct LinkAndRunInfo

struct InfoTable
{
    using list_type = std::unordered_map<std::string, std::shared_ptr<BaseInfo>>;
 
    template<typename T> void set(const T &rhs) {
        _data[T::name()] = std::make_shared<T>(rhs);
    }
 
    template<typename T> T *get(bool create_if_nx = false) {
        if (create_if_nx == false) {
            auto fd = _data.find(T::name());
            return (fd == _data.end())? nullptr: (T*)fd->second.get();
        }
        auto &uptr = _data.insert({T::name(), nullptr}).first->second;
        return (T*)(uptr? uptr : (uptr=std::shared_ptr<T>(new T))).get();
    }

    const list_type &data() const { return _data; }
    list_type &data() { return _data; }

    bool empty() const { return _data.empty(); }

    CGN_EXPORT void merge_from(const InfoTable &rhs);

    CGN_EXPORT void merge_entry(const std::string &name, const BaseInfo *rhs);

protected:
    list_type _data;
}; //struct InfoTable

// The result of API.analyse_target()
struct CGNTarget : InfoTable
{
    std::string factory_label;

    Configuration trimmed_cfg;

    GraphNode *anode;

    std::string errmsg;

    // the ninja target name
    std::string ninja_entry;

    constexpr static char NINJA_LEVEL_FULL   = 2;
    constexpr static char NINJA_LEVEL_DYNDEP = 1;
    constexpr static char NINJA_LEVEL_NONEED = 0;
    char ninja_dep_level = 0; //'f'ull, 'd'yndep or 'n'o-need

    // OS specific path separator
    // relavent path : the files/folders relavent to WorkingRoot
    // absolute path : allow any type of formats, as long as the downstream
    //                 interpreter supports.
    std::vector<std::string> outputs;

    //@param type: 'j': json_full (not implemented)
    //             'h': human readable text with size=5
    //             'H': fully human readable data
    CGN_EXPORT std::string to_string(char type = 'h') const;

}; //struct CGNTarget

// declare later
struct CGNTargetOpt;

// Parameter for user factory function and interpreter
struct CGNTargetOptIn
{
    // target BUILD_ENTRY
    // In the build.ninja file, a ‘target’ might be associated with an extensive 
    // number of files. To address this, we’ve established a single, consolidated 
    // entry point called ‘BUILD_ENTRY’ to initiate the compilation of the entire target.
    constexpr static const char BUILD_NINJA[] = "build.ninja",
                                BUILD_ENTRY[] = ".stamp";

    // factory_label : "//demo:hello_world"
    // factory_name  : "hello_world"
    const std::string &factory_name;
    const std::string &factory_label;

    Configuration &cfg;

    // a relative path that trailing with '/' (unix-separator)
    // like 'project1/'
    std::string src_prefix;

    std::vector<std::string> quickdep_ninja_dynhdr, quickdep_ninja_full;
    std::vector<GraphNode*>  quickdep_early_anodes;
    CGN_EXPORT CGNTarget quick_dep(const std::string &label, const Configuration &cfg, bool merge_infos = true);

    CGN_EXPORT CGNTargetOpt *confirm();

    CGN_EXPORT void confirm_with_error(const std::string &errmsg);

protected:
    // Only allocate by CGNTargetOpt
    CGNTargetOptIn(const std::string &name, const std::string &label, Configuration &cfg) 
    : factory_name(name), factory_label(label), cfg(cfg) {};

    virtual ~CGNTargetOptIn() {}

}; //struct CGNTargetOptIn


// the final statement of CGNTargetOptIn 
struct CGNTargetOpt : CGNTargetOptIn
{
    CGN_EXPORT static std::string path_separator;

    // File operator for '<out_dir>/build.ninja'
    NinjaFile *ninja = nullptr;

    // Node for current target{factory_label + trimmed_config} 
    // with files[] '<out_dir>/build.ninja' and 'obj/.../libBUILD.so'
    // only for this target
    GraphNode *anode;

    // a relative path or absolute path that trailing with '/' or '\' (system-path-separator)
    // like "cgn-out/obj/project1_/hello_FFFF1234/" (linux) 
    //   or "D:\\project1_output\\" (win-abspath)
    std::string out_prefix;

    // if true, 'result' is filled from the last cache, so the user does not 
    // need to generate it. Both 'ninja' and 'anode' will remain nullptr, and 
    // 'file_unchanged' will be set to true.
    bool cache_result_found = false;
    CGNTarget result;

    // If true, the pointers 'ninja' and 'anode' will have valid values. The 
    // user should populate 'result' as usual, but no files need to be written 
    // to disk since 'build.ninja' and its dependencies remain unchanged.
    bool file_unchanged = false;

    CGN_EXPORT CGNTargetOptIn *create_sub_target(const std::string &name, bool as_result = false);

    // return nullptr if not confirmed.
    CGNTarget *get_real_result();

    CGNTargetOpt(const std::string &factory_name)
    : CGNTargetOptIn(factory_name, result.factory_label, result.trimmed_cfg) {}
    
    virtual ~CGNTargetOpt() {}

private: //function inaccessable in CGNTargetOptIn
    CGNTarget quick_dep(const std::string &label, const Configuration &cfg, bool merge_infos);
    CGNTargetOpt *confirm();
    void confirm_with_error(const std::string &errmsg);
}; //struct CGNTargetOpt

// C++ 11 do not support std::array .
template<size_t N> struct ConstLabelGroup {
    using data_type = const char*;
    data_type data[N];
    constexpr static size_t size() { return N; }
    const data_type *begin() const { return data; }
    const data_type *end() const { return data + N; }
};

} //namespace