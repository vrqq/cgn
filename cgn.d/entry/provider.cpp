#include "../cgn.h"
#include "../provider_dep.h"

namespace cgn {

const BaseInfo::VTable DefaultInfo::v = {
    [](void *ecx, const void *rhs) {
        auto *self = (DefaultInfo *)ecx;
        auto *rr   = (const DefaultInfo *)(rhs);
        self->outputs.insert(self->outputs.end(), rr->outputs.begin(), rr->outputs.end());
    },
    [](const void *ecx) -> std::string { 
        return "DefaultInfo{...}";
    }
}; 

const BaseInfo::VTable LinkAndRunInfo::v = {
    [](void *ecx, const void *rhs) {
        auto *self = (LinkAndRunInfo *)ecx;
        auto *rr   = (const LinkAndRunInfo *)(rhs);
        auto merge = [](auto *out, auto &in) {
            out->insert(out->end(), in.begin(), in.end());
        };
        merge(&self->shared_files, rr->shared_files);
        merge(&self->static_files, rr->static_files);
        merge(&self->object_files, rr->object_files);
        self->runtime_files.insert(
            rr->runtime_files.begin(), rr->runtime_files.end());
    },
    [](const void *ecx) -> std::string { 
        return "LinkAndRunInfo{...}";
    }
}; 

void TargetInfos::merge_from(const TargetInfos &rhs)
{
    for (auto &[name, val] : rhs._data)
        if (auto fd = _data.find(name); fd != _data.end())
            fd->second->merge_from(val.get());
        else
            _data[name] = val;
}

std::string TargetInfos::to_string() const
{
    return "TargetInfos[...]";
}

// TargetInfoDep
// -------------

template<bool ConstCfg>
TargetInfoDep<ConstCfg>::TargetInfoDep(const Configuration &cfg, CGNTargetOpt opt) 
: cfg(cfg), TargetInfoDepData(opt) {
    auto *def = merged_info.get<DefaultInfo>(true);
    def->target_label = opt.factory_ulabel;
    def->build_entry_name = opt.out_prefix + opt.BUILD_ENTRY;
}

template<bool ConstCfg>
TargetInfos TargetInfoDep<ConstCfg>::add_dep(
    const std::string &label, Configuration cfg
) {
    auto rv = api.analyse_target(
        api.absolute_label(label, opt.src_prefix), this->cfg);
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