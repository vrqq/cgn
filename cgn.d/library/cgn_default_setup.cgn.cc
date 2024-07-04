#include "../cgn.h"
#include "cgn_default_setup.cgn.h"

CGN_EXPORT void cgn_setup(CGNInitSetup &x) {
    
    x.configs["host_release"] = generate_host_release();

    // api.get_kvargs() return arguments from command line, for example
    // api.get_kvargs()["target"]=="xxyy" from "./cgn --target xxyy"
    auto fdarg = api.get_kvargs().find("target");
    if (fdarg != api.get_kvargs().end()){
        x.log_message = "Target: " + fdarg->second;
        auto ls = str_to_set(fdarg->second);
        x.configs["DEFAULT"] = config_guessor(ls);
        if (ls.size()) { // the unused values are remained.
            x.log_message = "Unsupported options: ";
            for (auto it : ls)
                x.log_message += it + " ";
        }
    }
    else
        x.configs["DEFAULT"] = x.configs["host_release"];
}
