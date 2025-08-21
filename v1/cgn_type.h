// cgn.d public
// === CGN Public Type define ===
//
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <array>
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
// using FileLayout = std::map<std::string, std::string>;

// There's 3 types of usage, a special case: 
// CGNPath::type is meanless when CGNPath::rpath is abspath.
//  * path base on dir of current BUILD.cgn.cc
//  * path base on working_root (CWD, where cgn.exe running)
//  * path base on output dir of current target
struct CGNPath
{
    struct Hasher {
        std::size_t operator()(const CGNPath &p) const {
            return std::hash<std::string>()(p.rpath);
        }
    };

    enum RelType: char {
        BASE_ON_OUTPUT = 0,
        BASE_ON_SCRIPT = 1,
        BASE_ON_WORKINGROOT = 2,
    }type = BASE_ON_SCRIPT;

    std::string rpath;

    CGNPath(const char *rel) : rpath(rel) {}
    CGNPath(const std::string &rel = "") : rpath(rel) {}
    CGNPath(RelType t, const std::string &rel) : type(t), rpath(rel) {}

    std::string to_string() const {
        if (type == BASE_ON_OUTPUT)
            return "$(OUT_PREFIX)" + rpath;
        if (type == BASE_ON_SCRIPT)
            return "$(SCRIPT_DIR)" + rpath;
        return rpath.size()?rpath:".";
    }

    bool operator==(const CGNPath &rhs) const { return rpath == rhs.rpath && type == rhs.type;}
    bool operator!=(const CGNPath &rhs) const { return !(*this == rhs);}
};

inline CGNPath make_path_base_out(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_OUTPUT, rel};
}
inline CGNPath make_path_base_script(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_SCRIPT, rel};
}
inline CGNPath make_path_base_working(const std::string rel="")  { 
    return CGNPath{CGNPath::BASE_ON_WORKINGROOT, rel};
}

// CGNPathArray, std::vector<CGNPath> with some extra operator
class CGNPathArray : public std::vector<CGNPath>
{
public:
    CGNPathArray &operator=(const std::vector<std::string> &rhs) {
        this->clear();
        for (auto p1 : rhs)
            this->push_back(make_path_base_script(p1));
        return *this;
    }
    CGNPathArray &operator=(std::vector<std::string> &&rhs) {
        this->clear();
        for (auto p1 : rhs)
            this->push_back(make_path_base_script(std::move(p1)));
        return *this;
    }
    CGNPathArray &operator=(const std::initializer_list<CGNPath> &rhs) {
        *(std::vector<CGNPath>*)this = rhs;
        return *this;
    }

    CGNPathArray operator+(const std::vector<std::string> &rhs) {
        CGNPathArray lhs = *this;
        for (auto &p1 : rhs)
            lhs.push_back(make_path_base_script(p1));
        return lhs;
    }
    CGNPathArray operator+(std::vector<std::string> &&rhs) {
        CGNPathArray lhs = *this;
        for (auto &p1 : rhs)
            lhs.push_back(make_path_base_script(std::move(p1)));
        return lhs;
    }
    CGNPathArray operator+(const CGNPathArray &rhs) {
        CGNPathArray lhs = *this;
        lhs.insert(lhs.end(), rhs.begin(), rhs.end());
        return lhs;
    }

    CGNPathArray &operator+=(const std::vector<std::string> &rhs) {
        for (auto &p1 : rhs)
            this->push_back(make_path_base_script(p1));
        return *this;
    }

    CGNPathArray &operator+=(std::initializer_list<std::string> rhs) {
        for (auto &p1 : rhs)
            this->push_back(make_path_base_script(p1));
        return *this;
    }
    
    CGNPathArray &operator+=(const CGNPathArray &rhs) {
        this->insert(this->end(), rhs.begin(), rhs.end());
        return *this;
    }

    CGNPathArray &operator+=(CGNPathArray&& rhs) {
        this->insert(this->end(),
                     std::make_move_iterator(rhs.begin()),
                     std::make_move_iterator(rhs.end()));
        return *this;
    }
}; //class CGNPathArray

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
    // runtime_files[dest_dir] = src_file_abs-or-rel-path
    // e.g. For dll loaded by exe, dll.dest_dir usually shown as 'BASE_ON_OUTPUT + rel_path_of_exe'
    std::unordered_map<CGNPath, std::string, CGNPath::Hasher> runtime_files;
    // FileLayout runtime_files;

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

    // do not merge infos
    CGN_EXPORT CGNTarget quick_dep_namedcfg(const std::string &label, const std::string &cfgname, bool merge_cfg_visit = false);

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

template<size_t N>
using ConstLabelGroup = std::array<const char*, N>;

template<size_t NLeft, size_t NRight> ConstLabelGroup<NLeft + NRight> 
operator+(ConstLabelGroup<NLeft> lhs, ConstLabelGroup<NRight> rhs) {
    ConstLabelGroup<NLeft + NRight> rv;
    for (size_t i=0; i<NLeft; i++)
        rv[i] = lhs[i];
    for (size_t i=0; i<NRight; i++)
        rv[i + NLeft] = rhs[i];
    return rv;
}

} //namespace