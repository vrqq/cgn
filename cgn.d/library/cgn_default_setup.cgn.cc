#include "../cgn.h"
#include "cgn_default_setup.cgn.h"

void cgn_setup(CGNInitSetup &x) {
    
    x.configs["host_release"] = generate_host_release();

    auto fdarg = api.get_kvargs().find("target");
    if (fdarg != api.get_kvargs().end()){
        auto ls = str_to_set(fdarg->second);
        x.configs["DEFAULT"] = config_guessor(ls);
    }
    else
        x.configs["DEFAULT"] = x.configs["host_release"];
}
