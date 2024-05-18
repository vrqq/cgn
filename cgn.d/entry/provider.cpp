#include "../cgn.h"
#include "../provider_dep.h"

namespace {

template<typename Type1> std::string print1(const Type1 &ls, const std::string &indent)
{
    using Type = std::decay_t<decltype(ls)>;
    std::string ss;
    auto iter = ls.begin();
    for (size_t i=0; i<ls.size() && i<5; i++, iter++) {
        if (!ss.empty())
            ss += "\n" + indent;
        if constexpr(std::is_same_v<std::vector<std::string>, Type>)
            ss += *iter + ", ";
        else
            ss += iter->first + ": " + iter->second + ", ";
    }
    ss += "(" + std::to_string(ls.size()) + " elements)";
    return ss;
};

}

namespace cgn {

const BaseInfo::VTable DefaultInfo::v = {
    []() -> std::shared_ptr<BaseInfo> { 
        return std::make_shared<DefaultInfo>();
    },
    [](void *ecx, const void *rhs) {
        auto *self = (DefaultInfo *)ecx;
        auto *rr   = (const DefaultInfo *)(rhs);
        self->outputs.insert(self->outputs.end(), rr->outputs.begin(), rr->outputs.end());
    },
    [](const void *ecx) -> std::string { 
        auto *self = (DefaultInfo *)ecx;
        return std::string{"{\n"}
            + "  label: " + self->target_label + "\n"
            + " output: " + print1(self->outputs, "         ") + "\n"
            + "}";
    }
}; 

const BaseInfo::VTable LinkAndRunInfo::v = {
    []() -> std::shared_ptr<BaseInfo> { 
        return std::make_shared<LinkAndRunInfo>();
    },
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
        const LinkAndRunInfo *self = (const LinkAndRunInfo *)ecx;
        
        return std::string{"{\n"}
            + "    obj: " + print1(self->object_files, "         ") + "\n"
            + "    dll: " + print1(self->shared_files, "         ") + "\n"
            + "      a: " + print1(self->static_files, "         ") + "\n"
            + "     rt: " + print1(self->runtime_files, "         ") + "\n"
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
            ref->merge_from(val.get());
        }
}


void TargetInfos::merge_entry(
    const std::string &name, const std::shared_ptr<BaseInfo> &rhs
) {
    if (auto fd = _data.find(name); fd != _data.end())
        fd->second->merge_from(rhs.get());
    else {
        auto &ref = _data[name] = rhs->allocate();
        ref->merge_from(rhs.get());
    }
}


std::string TargetInfos::to_string() const
{
    std::string rv;
    for (auto &[name, inf] : _data)
        rv += "[" + name + "] " + inf->to_string() + "\n";
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