#define GENERAL_CGN_BUNDLE_IMPL
#include <map>
#include "../../cgn.h"
#include "bin_devel.cgn.h"

GENERAL_CGN_BUNDLE_API const cgn::BaseInfo::VTable*
BinDevelInfo::_glb_bindevel_vtable()
{
    const static cgn::BaseInfo::VTable v = {
        []() -> std::shared_ptr<cgn::BaseInfo> {
            return std::make_shared<BinDevelInfo>();
        },
        [](void *ecx, const cgn::BaseInfo *rhs) {
            BinDevelInfo *self = (BinDevelInfo*)ecx;
            if (self->base.empty() && self->include_dir.empty() && self->bin_dir.empty() && self->lib_dir.empty()) {
                *self = *(BinDevelInfo*)rhs;
                return true;
            }
            return false;
        }, 
        [](const void *ecx, char type) -> std::string { 
            auto *self = (BinDevelInfo *)ecx;
            return std::string{"{\n"}
                + "   base: " + self->base + "\n"
                + " incdir: " + self->include_dir + "\n"
                + " libdir: " + self->lib_dir + "\n"
                + "}";
        }
    };
    return &v; 
}
