#include <type_traits>
#include "cgn.h"

namespace cgn {
    
// The universal add_dep function for Interpreter
// ----------------------------------------------

struct TargetInfoDepData {
    const CGNTargetOpt opt;  //self opt
    std::vector<std::string> ninja_target_dep;
    TargetInfos              merged_info;
    TargetInfoDepData(const CGNTargetOpt &opt) : opt(opt) {}
};

template<bool ConstCfg = false>
struct TargetInfoDep : protected TargetInfoDepData {
    using cfg_type = typename std::conditional<ConstCfg, 
                        const Configuration, Configuration>::type;
    cfg_type cfg;

    TargetInfos add_dep(const std::string &label) {
        return add_dep(label, cfg);
    }
    TargetInfos add_dep(const std::string &label, Configuration cfg);

    TargetInfoDep(const Configuration &cfg, CGNTargetOpt opt);
};


}