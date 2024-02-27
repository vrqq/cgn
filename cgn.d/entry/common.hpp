#pragma once
#include <unordered_map>
#include <string>
#include <fstream>

inline std::unordered_map<std::string, std::string> 
read_kvfile(const std::string &fname)
{
    //string strip function
    auto strip = [](const std::string &ss) -> std::string {
        int i=0, j=ss.size()-1;
        while(ss[i] == ' ' && i<ss.size()) i++;
        while(ss[j] == ' ' && j>=i) j--;
        if (i <= j)
            return ss.substr(i, j-i+1);
        return "";
    };

    std::unordered_map<std::string, std::string> rv;
    std::ifstream fin(fname);
    for (std::string ss; !fin.eof() && std::getline(fin, ss);) {
        if (ss.empty())
            continue;
        if (auto fd = ss.find('='); fd != ss.npos) {
            std::string key = strip(ss.substr(0, fd-1));
            std::string val = strip(ss.substr(fd+1));
            if (key.size() && val.size())
                rv[key] = val;
        }
    }

    return rv;
}
