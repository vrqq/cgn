#include <filesystem>
#include "cgn_type.h"
#include "cgn_api.h"

namespace cgnv1 {

// === CGNTarget implement ===

void InfoTable::merge_from(const InfoTable &rhs)
{
    for (auto &[name, val] : rhs._data)
        if (auto fd = _data.find(name); fd != _data.end())
            fd->second->merge_entry(val.get());
        else {
            auto &ref = _data[name] = val->allocate();
            if (ref->merge_entry(val.get()) == false)
                _data.erase(name);
        }
}


void InfoTable::merge_entry(
    const std::string &name, const BaseInfo *rhs
) {
    if (auto fd = _data.find(name); fd != _data.end())
        fd->second->merge_entry(rhs);
    else {
        auto &ref = _data[name] = rhs->allocate();
        if (ref->merge_entry(rhs) == false)
            _data.erase(name);
    }
}


std::string CGNTarget::to_string(char type) const
{
    std::string rv;
    if (type == 'h' || type == 'H') {
        rv = "factory: " + this->factory_label
           + "#" + trimmed_cfg.get_id() + "\n";
        if (this->errmsg.size())
            rv += "error: " + this->errmsg;
        else {
            rv += "trimmed_cfg:\n    " + Logger::fmt_list(trimmed_cfg, "    ", 999) + "\n";
            rv += "ninja_target_entry: " + this->ninja_entry + "\n";
            rv += "ninja_depend_level: ";
            if (this->ninja_dep_level == NINJA_LEVEL_FULL)
                rv += "FULL\n";
            else if (this->ninja_dep_level == NINJA_LEVEL_DYNDEP)
                rv += "DYNDEP\n";
            else
                rv += "NONEED\n";

            rv += "OUTPUTS: (" + std::to_string(this->outputs.size()) + " elements)\n";
            for (auto it : this->outputs)
                rv += "    " + it + "\n";

            for (auto &[name, inf] : _data)
                rv += "[" + name + "] " + inf->to_string(type) + "\n";
        }
    }
    if (type == 'j') {}
    return rv;
}

// === CGNTargetOpt implement ===

//static variable
std::string CGNTargetOpt::path_separator = {std::filesystem::path::preferred_separator};

CGNTarget CGNTargetOptIn::quick_dep(const std::string &label, const Configuration &cfg1, bool merge_infos)
{
    CGNTargetOpt *opt = dynamic_cast<CGNTargetOpt*>(this);
    CGNTarget early = api.analyse_target(
                        api.absolute_label(label, this->factory_label), cfg1);
    if (early.errmsg.size())
        return early;
    if (early.ninja_dep_level == CGNTarget::NINJA_LEVEL_FULL)
        opt->quickdep_ninja_full.push_back(early.ninja_entry);
    if (early.ninja_dep_level == CGNTarget::NINJA_LEVEL_DYNDEP)
        opt->quickdep_ninja_dynhdr.push_back(early.ninja_entry);
    opt->quickdep_early_anodes.push_back(early.anode);
    cfg1.visit_keys(early.trimmed_cfg);
    if (merge_infos)
        opt->result.merge_from(early);
    return early;
}

CGNTarget CGNTargetOptIn::quick_dep_namedcfg(const std::string &label, const std::string &cfgname, bool merge_cfg_visit) {
    CGNTargetOpt *opt = dynamic_cast<CGNTargetOpt*>(this);
    auto early_cfg = api.query_config(cfgname);
    if (!early_cfg.second) {
        CGNTarget rv;
        rv.errmsg = "config '" + cfgname + "' not found";
        return rv;
    }
    CGNTarget rv = quick_dep(label, early_cfg.first, false);
    if (rv.errmsg.size())
        return rv;
    
    opt->quickdep_early_anodes.push_back(early_cfg.second);
    if (merge_cfg_visit)
        opt->cfg.visit_keys(rv.trimmed_cfg);
    return rv;
}

CGNTargetOpt *CGNTargetOptIn::confirm()
{
    return api.confirm_target_opt(this);
}


void CGNTargetOptIn::confirm_with_error(const std::string &errmsg)
{
    auto *opt = confirm();
    opt->result.errmsg = errmsg;
}

// These functions are defined in cgn_impl.cpp
// CGNTargetOptIn *CGNTargetOpt::create_sub_target(const std::string &name);
// CGNTarget *CGNTargetOpt::get_real_result();

// === LinkAndRunInfo implement ===

CGN_EXPORT const BaseInfo::VTable LinkAndRunInfo::v = {
    []() -> std::shared_ptr<BaseInfo> { 
        return std::make_shared<LinkAndRunInfo>();
    },
    [](void *ecx, const BaseInfo *rhs) {
        if (!rhs)
            return false;
        auto *self = (LinkAndRunInfo *)ecx;
        auto *rr   = (const LinkAndRunInfo *)(rhs);
        auto merge = [](auto *out, auto &in) {
            out->insert(out->end(), in.begin(), in.end());
            Tools::remove_duplicate_inplace(*out);
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
            + "  obj: " + Logger::fmt_list(self->object_files,  "       ", len) + "\n"
            + "  dll: " + Logger::fmt_list(self->shared_files,  "       ", len) + "\n"
            + "    a: " + Logger::fmt_list(self->static_files,  "       ", len) + "\n"
            + "   rt: " + Logger::fmt_list(self->runtime_files, "       ", len) + "\n"
            + "}";
    }
}; 

}