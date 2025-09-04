#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include "advcopy.h"

int show_help(char *arg0) {
    std::cerr
        <<"Usage: \n"
        <<arg0 <<" copy_to_dir      argfile.txt [logfile.txt]\n"
        <<arg0 <<" flat_copy_to_dir argfile.txt [logfile.txt]\n"
        <<arg0 <<" copy_rename      argfile.txt [logfile.txt]\n"
        <<arg0 <<" match <pattern_string>\n"
        <<arg0 <<" debug <pattern_string>\n"
        <<"Options in argfile:\n"
        <<"    @MF makefile_dependency_file.d\n"
        <<"    @stamp timestamp_file.stamp\n"
        <<"    @src   src or src_rel\n"
        <<"    @srcex src_exclude\n"
        <<"    @sbase src_base (copy_to_dir only)\n"
        <<"    @dst   dst_dir (copy_to_dir) or dst_file (copy_rename)\n"
        <<"Example argfile.txt of copy_to_dir()\n"
        <<" @MF out1.d\n"
        <<" @src src1\n"
        <<" @src src2\n"
        <<" @sbase src_base\n"
        <<" @dst dst_dir\n"
        <<std::endl;
    return 1;
}

int main(int argc, char **argv)
{
    // parse cli
    if (argc != 3 && argc != 4){
        std::cerr<<"Missing args, 2 or 3 args required, current is "<<argc-1<<std::endl;
        return show_help(argv[0]);
    }
    std::string arg1 = argv[1];
    std::string arg2 = argv[2];
    if (argc == 4)
        std::freopen(argv[3], "w", stdout);

    // parse argfile
    std::unordered_map<std::string, std::vector<std::string>> fn_args;
    std::ifstream fin(arg2);
    if (!fin) {
        std::cerr<<"Cannot open file " + arg2<<"\n";
        return show_help(argv[0]);
    }
    while(fin) {
        std::string ss;
        if (std::getline(fin, ss).eof())
            break;
        
        for (std::size_t i=0; i<ss.size(); i++){
            if (ss[i] == ' ')
                continue; //remove prefix ' '
            if (auto fd = ss.find(' ', i); fd != ss.npos)
                fn_args[ss.substr(i, fd-i)].push_back(ss.substr(fd+1));
            else
                fn_args[ss.substr(i)].push_back("");
        }
    }

    // select function
    if (arg1 == "match") {
        std::string errmsg;
        auto rv = cgnv1::AdvanceCopy::file_match(arg2, &errmsg);
        if (errmsg.size()) {
            std::cerr<<errmsg<<std::endl;
            return 1;
        }
        for (auto it : rv)
            std::cout<<it<<"\n";
        return 0;
    }
    if (arg1 == "debug") {
        std::string errmsg;
        cgnv1::AdvanceCopy::match_debug(arg2);
        return 0;
    }

    std::string depfile   = fn_args["MF"].empty()?"":fn_args["MF"][0];
    std::string stampfile = fn_args["stamp"].empty()?"":fn_args["stamp"][0];
    if (arg1 == "copy_to_dir") {
        if (fn_args["@src"].empty() || fn_args["@sbase"].empty() || fn_args["@dst"].empty()) {
            std::cerr<<"Missing src or dst in argfile\n";
            return 1;
        }
        
        if (auto emsg = cgnv1::AdvanceCopy::copy_to_dir(
            fn_args["@src"],
            fn_args["@srcex"],
            fn_args["@sbase"][0],
            fn_args["@dst"][0],
            depfile,
            stampfile,
            true
        ); emsg.size()) {
            std::cerr<<emsg<<std::endl;
            return 1;
        }
        return 0;
    }

    if (arg1 == "flat_copy_to_dir") {
        if (fn_args["@src"].empty() || fn_args["@dst"].empty()) {
            std::cerr<<"Missing src or dst in argfile\n";
            return 1;
        }

        if (auto emsg = cgnv1::AdvanceCopy::flatcopy_to_dir(
            fn_args["@src"],
            fn_args["@srcex"],
            fn_args["@dst"][0],
            depfile,
            stampfile,
            true
        ); emsg.size()) {
            std::cerr<<emsg<<std::endl;
            return 1;
        }
        return 0;
    }

    if (arg1 == "copy_rename") {
        if (fn_args["@src"].empty() || fn_args["@dst"].empty()) {
            std::cerr<<"Missing src or dst in argfile\n";
            return 1;
        }
        if (auto emsg = cgnv1::AdvanceCopy::copy_rename(
            fn_args["@src"][0],
            fn_args["@dst"][0],
            depfile,
            stampfile,
            true
        ); emsg.size()) {
            std::cerr<<emsg<<std::endl;
            return 1;
        }
        return 0;
    }

    // no function matched
    std::cerr<<"Unsupported command "<<arg1<<" "<<arg2<<std::endl;
    return show_help(argv[0]);
} //main()
