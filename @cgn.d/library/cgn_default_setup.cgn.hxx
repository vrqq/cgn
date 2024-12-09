#define CGN_SETUP_IMPL

#include "../cgn.h"
#include "cgn_default_setup.cgn.h"

void CGN_SETUP_IF cgn_setup(cgn::CGNInitSetup &x) {
    
    x.configs["host_release"] = generate_host_release();

    // api.get_kvargs() return arguments from command line, for example
    // api.get_kvargs()["target"]=="xxyy" from "./cgn --target xxyy"
    auto fdarg = api.get_kvargs().find("target");
    if (fdarg != api.get_kvargs().end()){
        x.log_message = "Target: " + fdarg->second;
        auto ls = str_to_set(fdarg->second);
        x.configs["DEFAULT"] = config_guessor(ls);
        if (ls.erase("host_tools_keep_same_config") == 1)
            x.configs["host_release"] = x.configs["DEFAULT"];
        if (ls.size()) { // the unused values are remained.
            x.log_message += "Unsupported options: ";
            for (auto it : ls)
                x.log_message += it + " ";
        }
    }
    else
        x.configs["DEFAULT"] = x.configs["host_release"];
    
    auto print_config = [&](std::string name){
        x.log_message += "\nconfig[" + name + "]:";
        for (auto it : x.configs[name])
            x.log_message += " " + it.first + "=" + it.second;
    };
    print_config("DEFAULT");
    print_config("host_release");
}
