#include "../cgn.h"
#include "../provider_dep.h"
#include "quick_print.hpp"

namespace cgn {

CGN_EXPORT const BaseInfo::VTable DefaultInfo::v = {
    []() -> std::shared_ptr<BaseInfo> { 
        return std::make_shared<DefaultInfo>();
    },
    [](void *ecx, const void *rhs) {
        return true;
        // auto *self = (DefaultInfo *)ecx;
        // auto *rr   = (const DefaultInfo *)(rhs);
        // self->outputs.insert(self->outputs.end(), rr->outputs.begin(), rr->outputs.end());
        // return true;
    },
    [](const void *ecx, char type) -> std::string { 
        auto *self = (DefaultInfo *)ecx;
        std::size_t len = (type=='h'?5:999);
        return std::string{"{\n"}
            + "   label: " + self->target_label + "\n"
            + "   entry: " + self->build_entry_name + "\n"
            + "  kepord: " + (self->enforce_keep_order?"True":"False") + "\n"
            + "  output: " + list2str_h(self->outputs, "          ", len) + "\n"
            + "}";
    }
}; 

// keep element first seen and remove duplicate in 'ls'
static void remove_dup(std::vector<std::string> &ls)
{
    std::unordered_set<std::string> visited;
    std::size_t i=0;
    for (std::size_t j=0; j<ls.size(); j++)
        if (visited.insert(ls[j]).second == true)
            std::swap(ls[i++], ls[j]);
    ls.resize(i);
}

CGN_EXPORT const BaseInfo::VTable LinkAndRunInfo::v = {
    []() -> std::shared_ptr<BaseInfo> { 
        return std::make_shared<LinkAndRunInfo>();
    },
    [](void *ecx, const void *rhs) {
        auto *self = (LinkAndRunInfo *)ecx;
        auto *rr   = (const LinkAndRunInfo *)(rhs);
        auto merge = [](auto *out, auto &in) {
            out->insert(out->end(), in.begin(), in.end());
            remove_dup(*out);
        };
        merge(&self->shared_files, rr->shared_files);
        merge(&self->static_files, rr->static_files);
        merge(&self->object_files, rr->object_files);
        self->runtime_files.insert(
            rr->runtime_files.begin(), rr->runtime_files.end());
        return true;
    },
    [](const void *ecx, char type) -> std::string { 
        const LinkAndRunInfo *self = (const LinkAndRunInfo *)ecx;
        
        std::size_t len = (type=='h'?5:999);
        return std::string{"{\n"}
            + "  obj: " + list2str_h(self->object_files,  "       ", len) + "\n"
            + "  dll: " + list2str_h(self->shared_files,  "       ", len) + "\n"
            + "    a: " + list2str_h(self->static_files,  "       ", len) + "\n"
            + "   rt: " + list2str_h(self->runtime_files, "       ", len) + "\n"
            + "}";
    }
}; 

void TargetInfos::merge_from(const TargetInfos &rhs)
{
    for (auto &[name, val] : rhs._data)
        if (auto fd = _data.find(name); fd != _data.end())
            fd->second->merge_from(val.get());
        else {
            auto &ref = _data[name] = val->allocate();
            if (ref->merge_from(val.get()) == false)
                _data.erase(name);
        }
}


void TargetInfos::merge_entry(
    const std::string &name, const std::shared_ptr<BaseInfo> &rhs
) {
    if (auto fd = _data.find(name); fd != _data.end())
        fd->second->merge_from(rhs.get());
    else {
        auto &ref = _data[name] = rhs->allocate();
        if (ref->merge_from(rhs.get()) == false)
            _data.erase(name);
    }
}


std::string TargetInfos::to_string(char type) const
{
    std::string rv;
    if (type == 'h' || type == 'H') {
        for (auto &[name, inf] : _data)
            rv += "[" + name + "] " + inf->to_string(type) + "\n";
    }
    return rv;
}

// TargetInfoDep
// -------------

template<bool ConstCfg>
TargetInfoDep<ConstCfg>::TargetInfoDep(const Configuration &cfg, CGNTargetOpt opt) 
: cfg(cfg), TargetInfoDepData(opt), name(this->opt.factory_name) {
    auto *def = merged_info.get<DefaultInfo>(true);
    def->target_label = opt.factory_ulabel;
    def->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;
}

template<bool ConstCfg>
TargetInfos TargetInfoDep<ConstCfg>::add_dep(
    const std::string &label, Configuration cfg
) {
    auto rv = api.analyse_target(
        api.absolute_label(label, opt.factory_ulabel), this->cfg);
    ninja_target_dep.push_back(rv.infos. template get<DefaultInfo>()->build_entry_name);
    merged_info.merge_from(rv.infos);
    
    // here we can use opt.adep directly and the output dir is fixed 
    // even if it would changed in interpreter.
    api.add_adep_edge(rv.adep, opt.adep);
    return rv.infos;
}

template TargetInfoDep<true>::TargetInfoDep(const Configuration &cfg, CGNTargetOpt opt);
template TargetInfoDep<false>::TargetInfoDep(const Configuration &cfg, CGNTargetOpt opt);

template TargetInfos TargetInfoDep<true>::add_dep(const std::string &label, Configuration cfg);
template TargetInfos TargetInfoDep<false>::add_dep(const std::string &label, Configuration cfg);

} //namespace cgn