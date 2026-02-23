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

    bool empty() const { return rpath.empty(); }
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
    using std::vector<CGNPath>::vector; // inherit all ctors

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
    CGNPathArray operator+(std::initializer_list<CGNPath> rhs) {
        CGNPathArray lhs = *this;
        lhs.insert(lhs.end(), rhs.begin(), rhs.end());
        return lhs;
    }

    CGNPathArray &operator+=(const std::vector<std::string> &rhs) {
        for (auto &p1 : rhs)
            this->push_back(make_path_base_script(p1));
        return *this;
    }

    // CGNPathArray &operator+=(std::initializer_list<std::string> rhs) {
    //     for (auto &p1 : rhs)
    //         this->push_back(make_path_base_script(p1));
    //     return *this;
    // }

    CGNPathArray &operator+=(std::initializer_list<CGNPath> rhs) {
        this->insert(this->end(), rhs.begin(), rhs.end());
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
    //shell: cmd, powershell, bash, zsh
    std::string os, cpu, shell;

    // gnu_get_libc_version() : 2.8
    // gnu_get_libc_release() : stable
    std::string glibc_version, glibc_release;
}; //struct HostInfo

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

    LinkAndRunInfo() : BaseInfo{&v} {}
    
    static const char *name() { return typeid(LinkAndRunInfo).name(); }

private:
    CGN_EXPORT static const VTable v;
}; //struct LinkAndRunInfo

// template<bool Ref = false>
struct InfoTable
{
    using list_type = std::unordered_map<std::string, std::shared_ptr<BaseInfo>>;
    
 
    template<typename T> void set(const T &rhs) {
        _data[typeid(T).name()] = std::make_shared<T>(rhs);
    }
 
    template<typename T> T *get(bool create_if_nx = false) {
        if (create_if_nx == false) {
            auto fd = _data.find(typeid(T).name());
            return (fd == _data.end())? nullptr: (T*)fd->second.get();
        }
        auto &sptr = _data.insert({typeid(T).name(), nullptr}).first->second;
        return (T*)(sptr? sptr : (sptr=std::shared_ptr<T>(new T))).get();
    }

    const list_type &data() const { return _data; }
    list_type &data() { return _data; }

    bool empty() const { return _data.empty(); }

    CGN_EXPORT bool merge_from(const InfoTable &rhs);

    CGN_EXPORT bool merge_entry(const std::string &name, const BaseInfo *rhs);

    template<typename T> bool merge_entry(const T *rhs) {
        return merge_entry(typeid(T).name(), rhs);
    }

    // template<bool RRef, typename = std::enable_if<Ref>::type>
    // InfoTable(InfoTable<RRef> &in) : _data(in._data) {}

    // InfoTable(std::enable_if<Ref, InfoTable<false>&>::type in) : _data(in._data) {}

protected:
    // using _underlying = std::conditional<Ref, list_type&, list_type>::type;
    // _data[typeid(T).name()] = shared_ptr<BaseInfo>
    // _underlying _data;
    list_type _data;
}; //struct InfoTable

// The result of API.analyse_target()
struct CGNTarget : InfoTable
{
    std::string label;

    Configuration trimmed_cfg;

    // Node for current target{factory_label + trimmed_config} 
    // with files[] '<out_dir>/build.ninja' and 'obj/.../libBUILD.so'
    // only for this target
    GraphNode *anode;

    std::string errmsg;

    // the ninja target name
    std::string ninja_entry;

    // TBD?
    // constexpr static char NINJA_LEVEL_FULL   = 2;
    // constexpr static char NINJA_LEVEL_DYNDEP = 1;
    // constexpr static char NINJA_LEVEL_NONEED = 0;
    // char ninja_dep_level = 0; //'f'ull, 'd'yndep or 'n'o-need

    // OS specific path separator
    // relavent path : the files/folders relavent to WorkingRoot
    // absolute path : allow any type of formats, as long as the downstream
    //                 interpreter supports.
    std::vector<std::string> outputs;

    //@param type: 'j': json_full (not implemented)
    //             'h': human readable text with size=5
    //             'H': fully human readable data
    CGN_EXPORT std::string to_string(char type = 'h') const;

    CGNTarget() {};
}; //struct CGNTarget

// declare later
struct CGNTargetOpt;
struct CGNTargetMaker;

// api.create_target() and api.active_script() calling stack record
struct TLRuntime {
    CGNTargetOpt *active_opt = nullptr;
    TLRuntime *call_from = nullptr;

    // For api.create_target(factory_name), label is full factory label
    //  @cell//src:target
    // For api.create_target(anonymous), label is shown as output base dir
    //  cgn-out/obj/anonymous_/target_FFFF1111
    // For api.active_script(), label is prefix
    //  @cell//dir
    std::string label;

    // function call dependency: graph.add_edge(dep_anodes[], current_anode)
    // TBD: rename to 'link_from_anode', 'edge_from_anode'
    std::unordered_set<GraphNode*> dep_anodes;

    //variable for create_target(), opt.confirm()
    std::string loop_detect_key;
    ConfigurationID cfgid_before_trim;

    //variable for opt.confirm()
    //  Only one of target_last or target_maker existed.
    //  target_last : pointer to api->targets[] (cache found)
    //  target_maker: api->target[] not exist.
    CGNTarget *target_now = nullptr;
    std::unique_ptr<CGNTargetMaker> target_maker;
};

// Parameter for user factory function and interpreter
// TBD: rename to CGNFactoryOpt?
struct CGNTargetOpt
{
    // "hello_world" for factory "//demo:hello_world".
    std::string name;

    Configuration cfg;

    // a relative path that trailing with '/' (unix-separator)
    // like 'project1/'
    std::string src_prefix;

    std::string out_parent_prefix;

    std::string out_parent_prefix_unixsep;

    std::string get_out_prefix_cfg0() const {
        return out_parent_prefix + name + "_00000000/";
    }

    // std::vector<std::string> quickdep_ninja_dynhdr, quickdep_ninja_full;
    // std::vector<GraphNode*>  quickdep_early_anodes;

    // Get current suggestion label
    // For named factory, the factory label returned.
    // For anonymous factory, the factory label is the base output dir (OS-sep)
    CGN_EXPORT const std::string &get_label() const;

    // if nullptr returned, last result was found or error occured, then user should return directly.
    CGN_EXPORT CGNTargetMaker *confirm();

    CGN_EXPORT void set_fail(const std::string &errmsg);

// protected:
//     // Only allocate by CGNTargetOpt
//     CGNTargetOptIn(const std::string &name, const std::string &label, Configuration &cfg) 
//     : factory_name(name), factory_label(label), cfg(cfg) {};

//     virtual ~CGNTargetOptIn() {}
private: friend class CGNImpl;
    class CGNImpl *_api_pimpl;
}; //struct CGNTargetOpt

// the final statement of CGNTargetOpt
struct CGNTargetMaker : CGNTarget
{
    // target BUILD_ENTRY
    // In the build.ninja file, a ‘target’ might be associated with an extensive 
    // number of files. To address this, we’ve established a single, consolidated 
    // entry point called NINJA_ENTRY_TARGET to initiate the compilation of the entire target.
    // const PATH_SEPARATOR       = "\\" (win) or "/" (xNIX)
    //       NINJA_ENTRY_FILENAME = "build.ninja"
    //       NINJA_ENTRY_TARGET   = ".stamp"
    CGN_EXPORT const static std::string PATH_SEPARATOR;
    CGN_EXPORT const static std::string NINJA_ENTRY_TARGET;
    CGN_EXPORT const static std::string NINJA_ENTRY_FILENAME;

    // from CGNTargetOpt.name
    const std::string &name;

    // from TLRuntime.label
    const std::string &label;

    // reference for CGNTarget, assigned by CGNImpl::confirm_target_opt()
    GraphNode * const &anode = CGNTarget::anode;

    const Configuration &trimmed_cfg = CGNTarget::trimmed_cfg;

    using CGNTarget::ninja_entry;

    const std::string &src_prefix;

    const std::string &out_parent_prefix;

    // a relative path or absolute path that trailing with '/' or '\' (system-path-separator)
    // like "cgn-out/obj/project1_/hello_FFFF1234/" (linux) 
    //   or "D:\\project1_output\\" (win-abspath)
    const std::string out_prefix;

    // output prefix with Unix style path separator
    // like "D:/project1_output/"
    const std::string out_prefix_unixsep;

    using CGNTarget::outputs;

    // File operator for '<out_dir>/build.ninja'
    // Inited by CGNImpl::confirm_target_opt()
    std::unique_ptr<NinjaFile> ninja;

    // other build script generated by current interpreter exclude build.ninja
    // OS path separator
    std::vector<std::string> ninja_file_appendix;

    // If true, the pointers 'ninja' and 'anode' will have valid values. The 
    // user should populate 'result' as usual, but no files need to be written 
    // to disk since 'build.ninja' and its dependencies remain unchanged.
    const bool &file_unchanged = _m_file_unchanged;

    CGNTargetMaker(CGNTargetOpt &opt)
    : name(opt.name),
      label(opt.get_label()),
      src_prefix(opt.src_prefix),
      out_parent_prefix(opt.out_parent_prefix), 
      out_prefix(opt.out_parent_prefix + opt.name + "_" + opt.cfg.get_id() + PATH_SEPARATOR), 
      out_prefix_unixsep(opt.out_parent_prefix_unixsep + opt.name + "_" + opt.cfg.get_id() + "/")
    {
        ((CGNTarget*)this)->trimmed_cfg = opt.cfg;
        ((CGNTarget*)this)->trimmed_cfg.visit_all_keys();
        ((CGNTarget*)this)->trimmed_cfg.trim_lock();
        ninja_entry = out_prefix + NINJA_ENTRY_FILENAME;
    };

private: friend class CGNImpl;
    bool _m_file_unchanged = false;
    std::string get_cache_name() {
        return out_parent_prefix + name + "_" + trimmed_cfg.get_id();
    }
}; //struct CGNTargetMaker


struct QuickDepContext
{
    // The collection from quickdep() and quick_dep_namedcfg()
    // quickdep_result : merged if parameter merge_result == true
    // quickdep_ninja_target : push_back for each CGNTarget.ninja_entry
    InfoTable quickdep_result;
    std::vector<std::string> quickdep_ninja_target;

    // Add dependency with specific config
    CGN_EXPORT CGNTarget quick_dep(const std::string &label, const Configuration &cfg, bool merge_result = true);

    // CGN_EXPORT CGNTarget quick_dep();

    // Add dependency with specific named config
    CGN_EXPORT CGNTarget quick_dep_namedcfg(const std::string &label, const std::string &cfgname, bool merge_result);

    QuickDepContext(CGNTargetOpt *current_opt) : opt(current_opt) {}

    CGNTargetOpt *opt;
};


// std::array<const char*, N> of C++ 11 implementation
// ---------------------------------------------------
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