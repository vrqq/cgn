#pragma once
#include <string>
#include <memory>
#include <unordered_set>
#include <unordered_map>

//Base class
struct BaseInfo {
    virtual void merge_from(const BaseInfo *rhs) = 0;
    virtual std::string to_string() const = 0;
    virtual ~BaseInfo() {}
};
inline std::ostream &operator<<(std::ostream &os, const BaseInfo &obj) {
    os<<obj.to_string();
    return os;
}

class TargetInfos
{
public:
    template<typename T> void set(T &rhs) { data[T::name()] = std::unique_ptr<T>(new T(rhs)); }
    template<typename T> T *get(bool create_if_nx = false) {
        auto &uptr = data.insert({T::name(), nullptr}).first->second;
        return (T*)(uptr? uptr : (uptr=std::unique_ptr<T>(new T))).get();
    }
    template<typename T> const T *get() const {
        auto fd = data.find(T::name());
        return (fd==data.end())? nullptr : fd->second.get();
    }
    void merge_from(const TargetInfos &rhs);
    void to_string() const;

private:
    std::unordered_map<std::string, std::unique_ptr<BaseInfo>> data;
}; //class TargetInfos

struct DefaultInfo : BaseInfo
{
    std::string target_label;

    //The files/folders relavent to WorkingRoot
    std::unordered_set<std::string> outputs;

    //The CGN script label in dependency tree (.cgn.cc or .cgn.rsp)
    std::unordered_set<std::string> dep_scripts;

    static const char *name() { return "DefaultInfo"; }
    virtual void merge_from(const BaseInfo *rhs) override {
        const DefaultInfo *rr = dynamic_cast<const DefaultInfo*>(rhs);
        outputs.insert(rr->outputs.begin(), rr->outputs.end());
        dep_scripts.insert(rr->dep_scripts.begin(), rr->dep_scripts.end());
    }
    virtual std::string to_string() const override { return name(); }
    virtual ~DefaultInfo() {}
};

//LinkingInfo
struct LinkingInfo : BaseInfo
{
    //The file path relavent to WorkingRoot
    std::unordered_set<std::string> lib_files;

    static const char *name() { return "LinkingInfo"; }

    virtual void merge_from(const BaseInfo *rhs) {
        const DefaultInfo *rr = dynamic_cast<const DefaultInfo*>(rhs);
        lib_files.insert(rr->outputs.begin(), rr->outputs.end());
    }
    virtual ~LinkingInfo() {}
};
