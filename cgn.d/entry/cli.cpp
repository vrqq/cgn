#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include "cgn_impl.h"
#include "../cgn.h"

static std::unique_ptr<CGNImpl> cgn_impl;
CGN glb{[](){
    if (!cgn_impl)
        cgn_impl = std::make_unique<CGNImpl>();
    return cgn_impl.get();
}()};

int show_helper(const char *arg0) {
    std::cerr<<arg0<<"\n"
             <<"     analyse <target_label>\n"
             <<"     build   <target_label>\n"
             <<"     run     <target_label>\n"
             <<"     clean\n"
             <<"  Options:\n"
             <<"     -C / --cgn-out + <dir_name>\n"
             <<"     -V / --verbose\n"
             <<"     --regeneration\n"
             <<std::endl;
    return 1;
}

int main(int argc, char **argv)
{
    //parse input cmdline
    std::vector<std::string> args;
    std::unordered_map<std::string, std::string> args_kv;
    for (int i=1; i<argc;) {
        auto argstart = [](const char *ss) { return (ss[0]=='-' && ss[1]=='-'); };
        if (argstart(argv[i])) {
            if (i+1 < argc && argstart(argv[i+1]))
                args_kv[argv[i] + 2] = argv[i+1], i+=2;
            else
                args_kv[argv[i] + 2] = "", i++;
        }
        else
            args.push_back(argv[i++]);
    }

    //init CGN
    if (auto fd = args_kv.find("cgn-out"); fd != args_kv.end()){
        if (fd->second.empty()) {
            std::cerr<<"Invalid cgn-out dir"<<std::endl;
            return show_helper(argv[0]);
        }
    }else
        args_kv["cgn-out"] = "cgn-out";

    //do command
    if (args.empty())
        return show_helper(argv[0]);
    
    auto get_cfg = [&]() {
        cgn_impl->init(args_kv);
        const Configuration *cfg = glb.query_config("DEFAULT");
        if (cfg)
            return *cfg;
        else {
            std::cerr<<"'DEFAULT' config not found."<<std::endl;
            exit(2);
        }
    };

    if (args[0] == "analyze" || args[0] == "analyse") {
        if (args.size() != 2)
            return show_helper(argv[0]);
        cgn_impl->analyse(args[1], get_cfg());
    }
    if (args[0] == "build") {
        if (args.size() != 2)
            return show_helper(argv[0]);
        cgn_impl->build(args[1], get_cfg());
    }
    if (args[0] == "clean"){
        std::filesystem::path dir{args_kv["cgn-out"]};
        if (std::filesystem::exists(dir / ".cgn_out_root.stamp"))
            std::filesystem::remove_all(dir);
        else
            std::cerr<<dir.string()<<"\n"
                     <<"Warning: it seems not a cgn-out folder"<<std::endl;
    }

    return 0;
}