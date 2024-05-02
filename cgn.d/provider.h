#pragma once
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "cgn.h"

namespace cgn {

// FileLayout[rel_path_of_output_dir] = path_to_origin_file_or_folder,
// the value accept both the relavent path of working root and absolute path.
using FileLayout = std::map<std::string, std::string>;

// C++ 11 do not support std::array .
template<size_t N> struct ConstLabelGroup {
    using data_type = const char*;
    data_type data[N];
    constexpr static size_t size() { return N; }
    const data_type *begin() const { return data; }
    const data_type *end() const { return data + N; }
};

//vtable of target info
struct TargetInfoV {
    void        (*merge_from)(void *ecx, void *rhs);
    std::string (*to_string)(void *ecx);
};

//Base class
struct BaseInfo {
    struct VTable {
        void        (*merge_from)(void *ecx, const void *rhs);
        std::string (*to_string)(const void *ecx);
    };

    BaseInfo(const VTable *vtable) : vtable(vtable) {};

    void merge_from(const BaseInfo *rhs) {
        return vtable->merge_from(this, rhs);
    }
    std::string to_string() const {
        return vtable->to_string(this);
    }

private:
    const VTable *vtable = nullptr;
};

struct DefaultInfo : BaseInfo
{
    std::string target_label;

    // the ninja target name
    std::string build_entry_name;

    //The files/folders relavent to WorkingRoot
    std::unordered_set<std::string> outputs;

    //The CGN script label in dependency tree (.cgn.cc or .cgn.rsp)
    // std::unordered_set<std::string> dep_scripts;

    DefaultInfo() : BaseInfo{&v} {}
    static const char *name() { return "DefaultInfo"; }

private:
    static const VTable v;
};

struct LinkAndRunInfo : BaseInfo {
    //The file path relavent to WorkingRoot
    std::vector<std::string> shared_files, static_files, object_files;
    
    // The files which not link directly but required when running.
    // It may the indirect dependency, and folder path also accepted.
    FileLayout runtime_files;

    LinkAndRunInfo() : BaseInfo{&v} {}
    
    static const char *name() { return "LinkAndRunInfo"; }

private:
    static const VTable v;
};

class TargetInfos
{
public:
    using list_type = std::unordered_map<std::string, std::shared_ptr<BaseInfo>>;

    template<typename T> void set(const T &rhs) { 
        _data[T::name()] = std::shared_ptr<T>(new T(rhs));
    }
    template<typename T> T *get(bool create_if_nx = false) {
        auto &uptr = _data.insert({T::name(), nullptr}).first->second;
        return (T*)(uptr? uptr : (uptr=std::shared_ptr<T>(new T))).get();
    }
    template<typename T> const T *get() const {
        auto fd = _data.find(T::name());
        return (fd==_data.end())? nullptr : fd->second.get();
    }

    const list_type &data() const { return _data; }

    bool empty() const { return _data.empty(); }

    void merge_from(const TargetInfos &rhs);

    std::string to_string() const;

    bool no_store = false;

private:
    list_type _data;
}; //class TargetInfos

} //namespace