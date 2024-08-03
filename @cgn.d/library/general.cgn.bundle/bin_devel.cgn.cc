#define GENERAL_CGN_BUNDLE_IMPL
#include "bin_devel.cgn.h"

GENERAL_CGN_BUNDLE_API const cgn::BaseInfo::VTable*
BinDevelInfo::_glb_bindevel_vtable()
{
    const static cgn::BaseInfo::VTable v = {
        []() -> std::shared_ptr<cgn::BaseInfo> {
            return std::make_shared<BinDevelInfo>();
        },
        [](void *ecx, const void *rhs) {
            return ;
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
