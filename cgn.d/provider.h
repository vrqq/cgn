#pragma once
#include <string>
#include <memory>
#include <map>
#include <unordered_set>
#include <unordered_map>

namespace cgn {

// FileLayout[rel_path_of_output_dir] = path_to_origin_file_or_folder,
// the value accept both the relavent path of working root and absolute path.
using FileLayout = std::map<std::string, std::string>;

//Base class
struct BaseInfo {
    static const char *name() { return "BaseInfo"; }
    virtual void merge_from(const BaseInfo *rhs) = 0;
    virtual std::string to_string() const = 0;
    virtual ~BaseInfo() {}
};
inline std::ostream &operator<<(std::ostream &os, const BaseInfo &obj) {
    os<<obj.to_string();
    return os;
}

struct DefaultInfo : BaseInfo
{
    std::string target_label;

    //The files/folders relavent to WorkingRoot
    std::unordered_set<std::string> outputs;

    //The CGN script label in dependency tree (.cgn.cc or .cgn.rsp)
    // std::unordered_set<std::string> dep_scripts;

    static const char *name() { return "DefaultInfo"; }
    virtual void merge_from(const BaseInfo *rhs) override {
        const DefaultInfo *rr = dynamic_cast<const DefaultInfo*>(rhs);
        outputs.insert(rr->outputs.begin(), rr->outputs.end());
        // dep_scripts.insert(rr->dep_scripts.begin(), rr->dep_scripts.end());
    }
    virtual std::string to_string() const override { return name(); }
    virtual ~DefaultInfo() {}
};

struct BuildAndRunInfo : BaseInfo {
    //The file path relavent to WorkingRoot
    std::vector<std::string> shared_files, static_files, object_files;
    
    // The files which not link directly but required when running.
    // It may the indirect dependency, and folder path also accepted.
    FileLayout runtime_files;

    static const char *name() { return "BuildAndRunInfo"; }
    virtual void merge_from(const BaseInfo *rhs);
    virtual std::string to_string() const;
    virtual ~BuildAndRunInfo() {}
};

class TargetInfos
{
public:
    using list_type = std::unordered_map<std::string, std::shared_ptr<BaseInfo>>;

    template<typename T> void set(T &rhs) { 
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

    // void merge_from(const TargetInfos &rhs);
    void to_string() const;

    bool no_store = false;

private:
    list_type _data;
}; //class TargetInfos

} //namespace