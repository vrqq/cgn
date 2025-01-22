#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include "advcopy.h"

int show_help(char *arg0) {
    std::cerr
        <<"Usage: \n"
        <<arg0 <<" copy_to_dir      [options...] filelist.txt\n"
        <<arg0 <<" flat_copy_to_dir [options...] filelist.txt\n"
        <<arg0 <<" copy_rename      [options...] filelist.txt\n"
        <<arg0 <<" match <pattern_string>\n"
        <<arg0 <<" debug <pattern_string>\n"
        <<"Options:\n"
        <<"    -MF file      set Makefile dependency file\n"
        <<"    -stamp file   set timestamp file\n"
        <<"filelist.txt:\n"
        <<"    The order same with function call.\n"
        <<std::endl;
    return 1;
}

int main(int argc, char **argv)
{
    // parse cli
    std::unordered_map<std::string, std::string> args_kv;
    std::vector<std::string> args_str;
    for (int i=1; i<argc; i++) {
        if (argv[i][0] == '-') {
            args_kv[argv[i]] = (i+1<argc)?argv[i+1]:"";
            i++; // extra i++
        }
        else
            args_str.push_back(argv[i]);
    }

    // parse filelist.txt and call function
    if (args_str.empty())
        return show_help(argv[0]);
    if (args_str.size() == 2 && args_str[0] == "match") {
        std::string errmsg;
        auto rv = cgnv1::AdvanceCopy::file_match(args_str[1], &errmsg);
        if (errmsg.size()) {
            std::cerr<<errmsg<<std::endl;
            return 1;
        }
        for (auto it : rv)
            std::cout<<it<<"\n";
        return 0;
    }
    if (args_str.size() == 2 && args_str[0] == "debug") {
        std::string errmsg;
        cgnv1::AdvanceCopy::match_debug(args_str[1]);
        return 0;
    }
    if (args_str.size() == 2) {
        std::string depfile   = args_kv["-MF"];
        std::string stampfile = args_kv["-stamp"];
        std::vector<std::string> filelist;

        std::ifstream fin(args_str[1]);
        if (!fin) {
            std::cerr<<"Cannot open file " + args_str[1]<<"\n";
            return show_help(argv[0]);
        }
        while(fin) {
            std::string ss;
            if (std::getline(fin, ss).eof())
                break;
            filelist.push_back(ss);
        }

        if (args_str[0] == "copy_to_dir") {
            if (filelist.size() < 3) {
                std::cerr<<args_str[1]<<" insufficient args\n";
                return 1;
            }
            std::string dst_dir = filelist.back();
            filelist.pop_back();

            std::string src_base = filelist.back();
            filelist.pop_back();

            if (auto emsg = cgnv1::AdvanceCopy::copy_to_dir(
                filelist, src_base, dst_dir, depfile, stampfile, true
            ); emsg.size()) {
                std::cerr<<emsg<<std::endl;
                return 1;
            }
            return 0;
        }

        if (args_str[0] == "flat_copy_to_dir") {
            if (filelist.size() < 2) {
                std::cerr<<args_str[1]<<" insufficient args\n";
                return 1;
            }

            std::string dst_dir = filelist.back();
            filelist.pop_back();

            if (auto emsg = cgnv1::AdvanceCopy::flatcopy_to_dir(
                filelist, dst_dir, depfile, stampfile, true
            ); emsg.size()) {
                std::cerr<<emsg<<std::endl;
                return 1;
            }
            return 0;
        }
    }

    return show_help(argv[0]);
} //main()
