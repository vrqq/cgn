#include "../provider.h"

namespace cgn {

const BaseInfo::VTable DefaultInfo::v = {
    [](void *ecx, const void *rhs) {
        auto *self = (DefaultInfo *)ecx;
        auto *rr   = (const DefaultInfo *)(rhs);
        self->outputs.insert(rr->outputs.begin(), rr->outputs.end());
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

std::string TargetInfos::to_string() const
{
    return "TargetInfos[...]";
}

}