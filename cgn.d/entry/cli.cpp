#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <filesystem>
#include "../cgn.h"

cgn::CGN api;

int show_helper(const char *arg0) {
    std::cerr<<arg0<<"\n"
             <<"     analyse <target_label>\n"
             <<"     build   <target_label>\n"
             <<"     run     <target_label>\n"
             <<"     clean\n"
             <<"  Options:\n"
             <<"     -C / --api.out + <dir_name>\n"
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
        std::string_view k{argv[i]};
        if (k[0] == '-') {
            //try expand
            if (k == "-C")
                k = "--cgn-out";
            else if (k == "-V")
                k = "--verbose";

            if (i+1 < argc && argv[i+1][0] != '-')
                args_kv[k.substr(2).data()] = argv[i+1], i+=2;
            else
                args_kv[k.substr(2).data()]= "", i++;
        }
        else
            args.push_back(argv[i++]);
    }

    //argument check
    if (auto fd = args_kv.find("cgn-out"); fd != args_kv.end()){
        if (fd->second.empty()) {
            std::cerr<<"Invalid cgn-out dir"<<std::endl;
            return show_helper(argv[0]);
        }
    }else
        args_kv["cgn-out"] = "cgn-out";

    if (args.empty())
        return show_helper(argv[0]);
    
    auto init0 = [&]() {
        api.init(args_kv);
        const auto *cfg = api.query_config("DEFAULT");
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
        api.init(args_kv);
        api.analyse_target(args[1], *api.query_config("DEFAULT"));
    }
    if (args[0] == "build") {
        if (args.size() != 2)
            return show_helper(argv[0]);
        api.init(args_kv);
        api.build(args[1], *api.query_config("DEFAULT"));
    }
    if (args[0] == "clean") {
        std::filesystem::path dir{args_kv["cgn-out"]};
        if (std::filesystem::exists(dir / ".cgn_out_root.stamp"))
            std::filesystem::remove_all(dir);
        else
            std::cerr<<dir.string()<<"\n"
                     <<"Warning: it seems not a cgn-out folder"<<std::endl;
    }

    return 0;
}